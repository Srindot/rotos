package dev.rotos.app.link

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.ParcelUuid
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeoutOrNull
import java.util.UUID

/**
 * BLE GATT link to ROTOS (Nordic-UART layout, docs/PROTOCOL.md):
 * scan for "ROTOS" / the service UUID, connect, request MTU 517, enable
 * notifications on TX (6e400003), write frames without response to RX
 * (6e400002) in pieces of MTU-3 bytes, one write in flight at a time.
 * Runtime permissions (BLUETOOTH_SCAN / CONNECT) are requested by the UI.
 */
@SuppressLint("MissingPermission")
/** [gentle]: ROTOS has been away a long time (bike parked): low-power scans, easy on the battery. */
class BleLink(private val context: Context, private val gentle: Boolean = false) : RotosLink {
    companion object {
        val SERVICE: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
        val RX_CHAR: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
        val TX_CHAR: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
        val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        const val NAME = "ROTOS"
        const val MTU = 517                 // big writes: fewer, faster map pieces
        private const val SCAN_TIMEOUT_MS = 15_000L
        private const val SETUP_TIMEOUT_MS = 15_000L
        private const val WRITE_TIMEOUT_MS = 2_000L
        /** Sends failing in a row before the link counts as dropped. */
        private const val MAX_FAILED_SENDS = 3
        /** The largest attribute value BLE allows (ROTOS rejects longer writes). */
        private const val MAX_WRITE = 512
    }

    private val _state = MutableStateFlow(LinkState.IDLE)
    override val state: StateFlow<LinkState> = _state
    private val _incoming = MutableSharedFlow<ByteArray>(extraBufferCapacity = 256, onBufferOverflow = BufferOverflow.DROP_OLDEST)
    override val incoming: SharedFlow<ByteArray> = _incoming
    override var peer: String = "BLE"
        private set

    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    @Volatile private var mtu = 23
    private var ready: CompletableDeferred<Result<Unit>>? = null
    private val writeDone = Channel<Int>(1, BufferOverflow.DROP_OLDEST)
    private val writeLock = Mutex()
    @Volatile private var dropped = false
    private var failStreak = 0

    /** Never throws: Bluetooth off / scan errors come back as failures; the GATT is closed on any failure or cancel. */
    override suspend fun connect(): Result<Unit> {
        _state.value = LinkState.CONNECTING
        dropped = false
        val found = runCatching { withTimeoutOrNull(SCAN_TIMEOUT_MS) { scan() } }
        if (found.isFailure && found.exceptionOrNull() is kotlinx.coroutines.CancellationException) throw found.exceptionOrNull()!!
        val device = found.getOrElse { return fail("scan failed: ${it.message}") }
            ?: return fail("no ROTOS found (scan ${SCAN_TIMEOUT_MS / 1000} s)")
        peer = device.address
        val done = CompletableDeferred<Result<Unit>>()
        ready = done
        var ok = false
        try {
            gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
            var r = withTimeoutOrNull(SETUP_TIMEOUT_MS) { done.await() } ?: Result.failure(IllegalStateException("GATT setup timeout"))
            if (r.isSuccess && dropped) r = Result.failure(IllegalStateException("disconnected during setup"))
            ok = r.isSuccess
            _state.value = if (ok) LinkState.CONNECTED else LinkState.FAILED
            if (ok && dropped) {                             // the drop raced with the line above
                _state.value = LinkState.DISCONNECTED
            }
            return r
        } finally {
            if (!ok) close()                                   // also when cancelled mid-connect
        }
    }

    private fun fail(msg: String): Result<Unit> {
        _state.value = LinkState.FAILED
        return Result.failure(IllegalStateException(msg))
    }

    private suspend fun scan(): BluetoothDevice {
        val mgr = context.getSystemService(BluetoothManager::class.java)
        val scanner = mgr?.adapter?.bluetoothLeScanner ?: throw IllegalStateException("Bluetooth off")
        val found = CompletableDeferred<BluetoothDevice>()
        val cb = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult) {
                found.complete(result.device)
            }
            override fun onScanFailed(errorCode: Int) {
                found.completeExceptionally(IllegalStateException("scan failed $errorCode"))
            }
        }
        val filters = listOf(
            ScanFilter.Builder().setDeviceName(NAME).build(),
            ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE)).build(),
        )
        val mode = if (gentle) ScanSettings.SCAN_MODE_LOW_POWER else ScanSettings.SCAN_MODE_LOW_LATENCY
        val settings = ScanSettings.Builder().setScanMode(mode).build()
        scanner.startScan(filters, settings, cb)
        try {
            return found.await()
        } finally {
            runCatching { scanner.stopScan(cb) }
        }
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)   // short interval: faster maps
                g.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                dropped = true                               // seen by connect() even before it says CONNECTED
                ready?.complete(Result.failure(IllegalStateException("disconnected (status $status)")))
                if (_state.value == LinkState.CONNECTED) _state.value = LinkState.DISCONNECTED
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val svc = g.getService(SERVICE)
            if (svc == null) {
                ready?.complete(Result.failure(IllegalStateException("ROTOS service missing")))
                return
            }
            rx = svc.getCharacteristic(RX_CHAR)
            g.requestMtu(MTU)
        }

        override fun onMtuChanged(g: BluetoothGatt, newMtu: Int, status: Int) {
            mtu = if (status == BluetoothGatt.GATT_SUCCESS) newMtu else 23
            val tx = g.getService(SERVICE)?.getCharacteristic(TX_CHAR)
            if (tx == null || rx == null) {
                ready?.complete(Result.failure(IllegalStateException("ROTOS characteristics missing")))
                return
            }
            g.setCharacteristicNotification(tx, true)
            val d = tx.getDescriptor(CCCD)
            if (d == null) {
                ready?.complete(Result.success(Unit))
                return
            }
            writeDescriptor(g, d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            ready?.complete(
                if (status == BluetoothGatt.GATT_SUCCESS) Result.success(Unit)
                else Result.failure(IllegalStateException("enable notifications failed $status")),
            )
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            writeDone.trySend(status)
        }

        @Deprecated("API < 33 path")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION")
            c.value?.let { _incoming.tryEmit(it.copyOf()) }
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) {
            _incoming.tryEmit(value.copyOf())
        }
    }

    @Suppress("DEPRECATION")
    private fun writeDescriptor(g: BluetoothGatt, d: BluetoothGattDescriptor, v: ByteArray) {
        if (Build.VERSION.SDK_INT >= 33) {
            g.writeDescriptor(d, v)
        } else {
            d.value = v
            g.writeDescriptor(d)
        }
    }

    @Suppress("DEPRECATION")
    private fun writeChar(g: BluetoothGatt, c: BluetoothGattCharacteristic, v: ByteArray): Boolean {
        val type = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
        return if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(c, v, type) == BluetoothGatt.GATT_SUCCESS
        } else {
            c.writeType = type
            c.value = v
            g.writeCharacteristic(c)
        }
    }

    override suspend fun send(bytes: ByteArray): Result<Unit> = writeLock.withLock {
        val r = sendLocked(bytes)
        // writes that keep failing mean the link is gone even if Android never said so: let it reconnect
        failStreak = if (r.isSuccess) 0 else failStreak + 1
        if (failStreak >= MAX_FAILED_SENDS && _state.value == LinkState.CONNECTED) {
            _state.value = LinkState.DISCONNECTED
            runCatching { gatt?.disconnect() }
        }
        r
    }

    private suspend fun sendLocked(bytes: ByteArray): Result<Unit> {
        val g = gatt ?: return Result.failure(IllegalStateException("not connected"))
        val c = rx ?: return Result.failure(IllegalStateException("not connected"))
        while (writeDone.tryReceive().isSuccess) { /* a late completion of a timed-out write */ }
        val piece = (mtu - 3).coerceIn(20, MAX_WRITE)       // ATT values are 512 bytes at most
        var off = 0
        for (i in 0 until bytes.size / piece + 1) {
            if (off >= bytes.size) break
            val part = bytes.copyOfRange(off, minOf(bytes.size, off + piece))
            if (!writeChar(g, c, part)) return Result.failure(IllegalStateException("BLE write rejected"))
            withTimeoutOrNull(WRITE_TIMEOUT_MS) { writeDone.receive() }
                ?: return Result.failure(IllegalStateException("BLE write timeout"))
            off += part.size
        }
        return Result.success(Unit)
    }

    override fun close() {
        runCatching { gatt?.disconnect() }
        runCatching { gatt?.close() }
        gatt = null
        rx = null
        if (_state.value != LinkState.FAILED) _state.value = LinkState.DISCONNECTED
    }
}
