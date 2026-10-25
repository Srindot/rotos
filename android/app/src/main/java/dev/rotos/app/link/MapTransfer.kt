package dev.rotos.app.link

import dev.rotos.app.nav.Ack
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import dev.rotos.app.nav.Packets
import kotlinx.coroutines.channels.ReceiveChannel
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.sync.withLock

data class TransferProgress(val acked: Int, val total: Int, val resends: Int)

/**
 * Sliding-window upload of the current map pack (docs/PROTOCOL.md "Map transfer"):
 *   MAP_BEGIN -> wait up to 10 s for ACK(MAP_BEGIN, OK)
 *   MAP_CHUNKs, never more than 16 chunks beyond the last acknowledged byte count
 *   ACK(MAP_CHUNK, OK, value) advances the window; BAD_STATE resumes from value
 *   3 s without progress -> resend from the last acknowledged value
 *   after the last chunk: MAP_END -> ACK(MAP_END, OK) = done, BAD_CRC = failure
 * [acks] carries every ACK the link receives (RideService's reader routes them).
 */
class MapTransfer(
    private val link: RotosLink,
    private val seq: SeqCounter,
    private val acks: ReceiveChannel<Ack>,
) {
    companion object {
        const val BEGIN_TIMEOUT_MS = 20_000L        // ROTOS erases its map flash first
        const val STALL_MS = 3_000L
        const val WINDOW_CHUNKS = 40               // 10 KB in flight; ROTOS ACKs every 8
        const val MAX_STALLS = 10
        private const val REWIND_HOLDOFF_MS = 1_000L
    }

    private class Window(val total: Int, val chunk: Int) {
        var acked = 0
        var next = 0
        var endSent = false
        var resends = 0
        var stalls = 0
        var lastProgress = now()
        var rewoundTo = -1
        var rewoundAt = 0L

        fun rewind(to: Int) {
            next = to
            endSent = false
            resends++
            rewoundTo = to
            rewoundAt = now()
            lastProgress = now()
        }
    }

    /** [total] = length of the current pack (PackInfo.length). */
    suspend fun send(total: Int, progress: (TransferProgress) -> Unit): Result<Unit> {
        for (i in 0 until 1024) if (acks.tryReceive().isFailure) break    // drop stale ACKs
        if (total <= 0) return fail("no map pack to send")
        sendFrame { NativeNav.frameMapBegin(seq.next(), it) }.onFailure { return Result.failure(it) }
        val begin = awaitAck(Packets.MAP_BEGIN, BEGIN_TIMEOUT_MS) ?: return fail("no ACK for MAP_BEGIN in 10 s")
        if (begin.status == Packets.ACK_HAVE) {                            // ROTOS already has this exact map
            progress(TransferProgress(total, total, 0))
            return Result.success(Unit)
        }
        if (begin.status == Packets.ACK_NO_BASE) return Result.failure(NoBase())
        if (begin.status != Packets.ACK_OK) return fail("ROTOS refused the map (status ${begin.status})")
        val w = Window(total, NavThread.run { NativeNav.mapChunkSize() })
        val maxRounds = (total / w.chunk + 1) * 20 + 1000
        for (round in 0 until maxRounds) {
            fillWindow(w).onFailure { return Result.failure(it) }
            val wait = maxOf(1L, STALL_MS - (now() - w.lastProgress))
            val a = withTimeoutOrNull(wait) { acks.receive() }
            if (a == null) {
                if (++w.stalls > MAX_STALLS) return fail("map transfer stalled at ${w.acked}/$total")
                w.rewind(w.acked)
                continue
            }
            val done = onAck(w, a).getOrElse { return Result.failure(it) }
            progress(TransferProgress(w.acked, total, w.resends))
            if (done) return Result.success(Unit)
        }
        return fail("map transfer did not finish")
    }

    /**
     * Tops the window up: all new chunk frames (plus MAP_END after the last)
     * are encoded in one NavThread task and written as one send, instead of a
     * thread hop and a write per 240-byte chunk (the old bottleneck).
     */
    private suspend fun fillWindow(w: Window): Result<Unit> {
        val offsets = ArrayList<Int>(WINDOW_CHUNKS)
        var next = w.next
        while (offsets.size < WINDOW_CHUNKS && !w.endSent && next < w.total && next < w.acked + WINDOW_CHUNKS * w.chunk) {
            offsets.add(next)
            next += w.chunk
        }
        val end = !w.endSent && next >= w.total
        if (offsets.isEmpty() && !end) return Result.success(Unit)
        val out = java.io.ByteArrayOutputStream((offsets.size + 1) * NativeNav.MAX_FRAME)
        seq.order.withLock {                            // numbered and sent in one go
            val bad = NavThread.run {
                val buf = ByteArray(NativeNav.MAX_FRAME)
                var err = 0
                for (off in offsets) {
                    val n = NativeNav.frameMapChunk(seq.next(), off, buf)
                    if (n <= 0) { err = n; break }
                    out.write(buf, 0, n)
                }
                if (err == 0 && end) {
                    val n = NativeNav.frameMapEnd(seq.next(), buf)
                    if (n <= 0) err = n else out.write(buf, 0, n)
                }
                err
            }
            if (bad != 0) return fail("frame build failed (rn_status $bad)")
            link.send(out.toByteArray()).onFailure { return Result.failure(it) }
        }
        w.next = next
        if (end) w.endSent = true
        return Result.success(Unit)
    }

    /** Applies one ACK; true when the transfer is complete. */
    private fun onAck(w: Window, a: Ack): Result<Boolean> {
        val value = a.value.coerceIn(0L, w.total.toLong()).toInt()
        when (a.ackedType) {
            Packets.MAP_CHUNK -> {
                if (value > w.acked) {
                    w.acked = value
                    w.lastProgress = now()
                    w.stalls = 0
                }
                val sameRewind = value == w.rewoundTo && now() - w.rewoundAt < REWIND_HOLDOFF_MS
                if (a.status == Packets.ACK_BAD_STATE && !sameRewind) {
                    w.acked = value
                    w.rewind(value)
                }
            }
            Packets.MAP_END -> return when (a.status) {
                Packets.ACK_OK -> { w.acked = w.total; Result.success(true) }
                Packets.ACK_BAD_CRC -> fail("ROTOS reports a bad CRC for the map")
                else -> { w.acked = value; w.rewind(value); Result.success(false) }
            }
            else -> Unit
        }
        return Result.success(false)
    }

    private suspend fun awaitAck(type: Int, timeoutMs: Long): Ack? = withTimeoutOrNull(timeoutMs) {
        var found: Ack? = null
        for (i in 0 until 1000) {
            val a = acks.receive()
            if (a.ackedType == type) { found = a; break }
        }
        found
    }

    private suspend fun sendFrame(build: (ByteArray) -> Int): Result<Unit> = seq.order.withLock {
        val buf = ByteArray(NativeNav.MAX_FRAME)
        val n = NavThread.run { build(buf) }
        if (n <= 0) fail("frame build failed (rn_status $n)") else link.send(buf.copyOf(n))
    }

    private fun <T> fail(msg: String): Result<T> = Result.failure(IllegalStateException(msg))

    /** ROTOS does not hold the pack the patch was made against: send the whole pack. */
    class NoBase : Exception("ROTOS does not hold the base pack of this patch")
}

private fun now(): Long = System.nanoTime() / 1_000_000L
