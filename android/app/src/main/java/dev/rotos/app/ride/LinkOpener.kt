package dev.rotos.app.ride

import android.content.Context
import dev.rotos.app.call.CallController
import dev.rotos.app.link.BleLink
import dev.rotos.app.link.ReconnectingLink
import dev.rotos.app.link.RotosLink
import dev.rotos.app.link.TcpLink
import dev.rotos.app.nav.Ack
import dev.rotos.app.nav.ButtonEvent
import dev.rotos.app.nav.CallAction
import dev.rotos.app.nav.DeviceStatus
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import dev.rotos.app.nav.Packets
import dev.rotos.app.nav.RxDecoder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.channels.SendChannel
import kotlinx.coroutines.delay

/** Opening the link to ROTOS, and routing what ROTOS sends back. */
object LinkOpener {
    private const val TRIES = 3
    private const val RETRY_MS = 2_000L
    private const val PERSIST_RETRY_MS = 30_000L                  // after ~10 min of searching: easier on the battery
    /** Failed searches in a row (~10 min) before scans go low-power: the bike is parked. */
    private const val GENTLE_AFTER = 30

    /**
     * Up to [TRIES] attempts over BLE or TCP ([persistent]: until found or
     * cancelled, for sessions Android restored); the link returned reconnects by
     * itself after drop-outs ([onUp] runs after the first connect and every
     * reconnect). Null when ROTOS was not found; the link is closed then.
     */
    suspend fun open(
        context: Context, scope: CoroutineScope, cfg: RideConfig,
        log: (String) -> Unit, persistent: Boolean, onTry: () -> Unit, onUp: () -> Unit,
    ): ReconnectingLink? {
        val l = ReconnectingLink(
            scope,
            factory = { misses -> if (cfg.link == LinkKind.TCP) TcpLink(cfg.host, cfg.port) else BleLink(context, gentle = misses >= GENTLE_AFTER) },
            onEvent = log,
            onReconnected = { NavThread.run { NativeNav.rxReset() }; onUp() },
        )
        var connected = false
        try {
            // a restored session (app restarted by Android) keeps looking: ROTOS may be off until ignition
            var attempt = 0
            while (persistent || attempt < TRIES) {
                attempt++
                onTry()
                if (attempt <= TRIES || attempt % 20 == 0) log("connecting to ROTOS over ${cfg.link} (try $attempt)")
                val r = l.connect()
                if (r.isSuccess) {
                    connected = true
                    NavThread.run { NativeNav.rxReset() }
                    onUp()
                    log("connected to ${l.peer}")
                    return l
                }
                if (attempt <= TRIES) log("connect failed: ${r.exceptionOrNull()?.message}")
                delay(if (attempt < 30) RETRY_MS else PERSIST_RETRY_MS)
            }
        } finally {
            if (!connected) l.close()                          // switch turned off while searching
        }
        log(if (cfg.link == LinkKind.BLE) "ROTOS not found" else "simulator not reachable at ${cfg.host}:${cfg.port}")
        return null
    }

    private class Inbound(val type: Int, val ack: Ack?, val button: ButtonEvent?, val call: CallAction?, val device: DeviceStatus?)

    /**
     * ROTOS -> phone: ACKs (map transfer to [acks], route lines to [lineAcks]),
     * button presses, call popup actions. Runs until cancelled.
     */
    suspend fun route(
        l: RotosLink, acks: SendChannel<Ack>, lineAcks: SendChannel<Ack>, calls: () -> CallController?,
        log: (String) -> Unit, device: (DeviceStatus) -> Unit = {},
    ) = coroutineScope {
        l.incoming.collect { bytes ->
            val events = NavThread.run {
                RxDecoder.feed(bytes).map { p ->
                    Inbound(p.type, RxDecoder.ack(p), RxDecoder.button(p), RxDecoder.callAction(p),
                        if (p.type == Packets.DEVICE_STATUS) DeviceStatus.parse(p.payload, p.len) else null)
                }
            }
            for (e in events) {
                when (e.type) {
                    Packets.ACK -> e.ack?.let { if (it.ackedType == Packets.ROUTE) lineAcks.trySend(it) else acks.trySend(it) }
                    Packets.BUTTON -> e.button?.let { log("BUTTON ${it.button} ${if (it.event == 2) "long" else "short"}") }
                    Packets.CALL_ACTION -> e.call?.let { a ->
                        log("CALL_ACTION id ${a.callId} action ${a.action}")
                        // off this loop: a reject waits up to 20 s for its SMS, ACKs must not wait behind it
                        calls()?.let { c -> launch { c.onAction(a) } } ?: log("call alerts disabled: action ignored")
                    }
                    Packets.DEVICE_STATUS -> e.device?.let(device)
                    Packets.PING -> Unit
                    else -> log("rx type 0x%02x ignored".format(e.type))
                }
            }
        }
    }
}
