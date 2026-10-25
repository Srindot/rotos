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
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import dev.rotos.app.nav.Packets
import dev.rotos.app.nav.RxDecoder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.channels.SendChannel
import kotlinx.coroutines.delay

/** Opening the link to ROTOS, and routing what ROTOS sends back. */
object LinkOpener {
    private const val TRIES = 3
    private const val RETRY_MS = 2_000L

    /**
     * Up to [TRIES] attempts over BLE or TCP; the link returned reconnects by
     * itself after drop-outs ([onUp] runs after the first connect and every
     * reconnect). Null when ROTOS was not found; the link is closed then.
     */
    suspend fun open(
        context: Context, scope: CoroutineScope, cfg: RideConfig,
        log: (String) -> Unit, onTry: () -> Unit, onUp: () -> Unit,
    ): ReconnectingLink? {
        val l = ReconnectingLink(
            scope,
            factory = { if (cfg.link == LinkKind.TCP) TcpLink(cfg.host, cfg.port) else BleLink(context) },
            onEvent = log,
            onReconnected = { NavThread.run { NativeNav.rxReset() }; onUp() },
        )
        var connected = false
        try {
            for (attempt in 1..TRIES) {
                onTry()
                log("connecting to ROTOS over ${cfg.link} (try $attempt)")
                val r = l.connect()
                if (r.isSuccess) {
                    connected = true
                    NavThread.run { NativeNav.rxReset() }
                    onUp()
                    log("connected to ${l.peer}")
                    return l
                }
                log("connect failed: ${r.exceptionOrNull()?.message}")
                delay(RETRY_MS)
            }
        } finally {
            if (!connected) l.close()                          // switch turned off while searching
        }
        log(if (cfg.link == LinkKind.BLE) "ROTOS not found" else "simulator not reachable at ${cfg.host}:${cfg.port}")
        return null
    }

    private class Inbound(val type: Int, val ack: Ack?, val button: ButtonEvent?, val call: CallAction?)

    /**
     * ROTOS -> phone: ACKs (map transfer to [acks], route lines to [lineAcks]),
     * button presses, call popup actions. Runs until cancelled.
     */
    suspend fun route(
        l: RotosLink, acks: SendChannel<Ack>, lineAcks: SendChannel<Ack>, calls: () -> CallController?,
        log: (String) -> Unit,
    ) {
        l.incoming.collect { bytes ->
            val events = NavThread.run {
                RxDecoder.feed(bytes).map { p -> Inbound(p.type, RxDecoder.ack(p), RxDecoder.button(p), RxDecoder.callAction(p)) }
            }
            for (e in events) {
                when (e.type) {
                    Packets.ACK -> e.ack?.let { if (it.ackedType == Packets.ROUTE) lineAcks.trySend(it) else acks.trySend(it) }
                    Packets.BUTTON -> e.button?.let { log("BUTTON ${it.button} ${if (it.event == 2) "long" else "short"}") }
                    Packets.CALL_ACTION -> e.call?.let { a ->
                        log("CALL_ACTION id ${a.callId} action ${a.action}")
                        calls()?.onAction(a) ?: log("call alerts disabled: action ignored")
                    }
                    Packets.PING -> Unit
                    else -> log("rx type 0x%02x ignored".format(e.type))
                }
            }
        }
    }
}
