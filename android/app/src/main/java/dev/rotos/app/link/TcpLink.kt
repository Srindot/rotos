package dev.rotos.app.link

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket

/** TCP to the simulator (`rotos_sim --listen 7400`); the emulator reaches the host at 10.0.2.2. */
class TcpLink(private val host: String, private val port: Int) : RotosLink {
    private companion object {
        const val CONNECT_TIMEOUT_MS = 5_000
    }

    private val _state = MutableStateFlow(LinkState.IDLE)
    override val state: StateFlow<LinkState> = _state
    private val _incoming = MutableSharedFlow<ByteArray>(extraBufferCapacity = 256, onBufferOverflow = BufferOverflow.SUSPEND)
    override val incoming: SharedFlow<ByteArray> = _incoming
    override val peer: String = "$host:$port"

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val writeLock = Mutex()
    private var socket: Socket? = null
    private var out: OutputStream? = null
    private var reader: Job? = null

    override suspend fun connect(): Result<Unit> = withContext(Dispatchers.IO) {
        _state.value = LinkState.CONNECTING
        val s = Socket()
        runCatching {
            s.tcpNoDelay = true
            s.connect(InetSocketAddress(host, port), CONNECT_TIMEOUT_MS)
            socket = s
            out = s.getOutputStream()
            reader = scope.launch { readLoop(s) }
            _state.value = LinkState.CONNECTED
        }.onFailure {
            runCatching { s.close() }                        // retried every few seconds: no socket leak
            _state.value = LinkState.FAILED
        }
    }

    private suspend fun readLoop(s: Socket) {
        val buf = ByteArray(4096)
        runCatching {
            val input = s.getInputStream()
            while (scope.isActive) {                       // bounded by the socket's lifetime
                val n = input.read(buf)
                if (n < 0) break
                if (n > 0) _incoming.emit(buf.copyOf(n))
            }
        }
        if (_state.value == LinkState.CONNECTED) _state.value = LinkState.DISCONNECTED
    }

    override suspend fun send(bytes: ByteArray): Result<Unit> = withContext(Dispatchers.IO) {
        writeLock.withLock {
            runCatching {
                val o = out ?: throw IllegalStateException("not connected")
                o.write(bytes)
                o.flush()
            }.onFailure { if (_state.value == LinkState.CONNECTED) _state.value = LinkState.DISCONNECTED }
        }
    }

    override fun close() {
        runCatching { socket?.close() }
        reader?.cancel()
        scope.cancel()
        socket = null
        out = null
        if (_state.value != LinkState.FAILED) _state.value = LinkState.DISCONNECTED
    }
}
