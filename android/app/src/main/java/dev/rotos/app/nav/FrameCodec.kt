package dev.rotos.app.nav

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * The two frames sent every second, NAV_STATE (position) and GUIDANCE
 * (Google's turn), encoded in Kotlin byte for byte like core/src/link
 * (rn_ffi_frame_position / rn_ffi_frame_guidance; FrameCodecTest pins it).
 * They never wait for NavThread, so positions keep flowing while a map is
 * being built. Pure.
 */
object FrameCodec {
    private const val VERSION = 2
    private const val NAV_FREE = 0x20
    private const val NAV_LEN = 38
    private const val GUIDE_LEN = 48
    private const val ROAD_MAX = 32
    private const val MANEUVER_COUNT = 20
    private const val MANEUVER_STRAIGHT = 2
    private const val UNKNOWN32 = 0xFFFFFFFFL

    /** CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). */
    fun crc16(b: ByteArray, from: Int, n: Int): Int {
        var c = 0xFFFF
        for (i in from until from + n) {
            c = c xor ((b[i].toInt() and 0xFF) shl 8)
            for (k in 0 until 8) c = if (c and 0x8000 != 0) ((c shl 1) xor 0x1021) and 0xFFFF else (c shl 1) and 0xFFFF
        }
        return c
    }

    fun frame(type: Int, seq: Int, payload: ByteArray): ByteArray {
        val out = ByteBuffer.allocate(10 + payload.size).order(ByteOrder.LITTLE_ENDIAN)
        out.put(0xA5.toByte()).put(0x5A.toByte()).put(VERSION.toByte()).put(type.toByte())
        out.putShort(seq.toShort()).putShort(payload.size.toShort()).put(payload)
        val a = out.array()
        val crc = crc16(a, 2, 6 + payload.size)
        a[a.size - 2] = crc.toByte()
        a[a.size - 1] = (crc shr 8).toByte()
        return a
    }

    private fun clampU16(v: Float): Int = if (!(v > 0f)) 0 else if (v >= 65535f) 65535 else v.toInt()

    /** NAV_STATE in free mode, like rn_ffi_frame_position. */
    fun position(
        seq: Int, tMs: Int, routeId: Int, lat: Double, lon: Double, speedMps: Float, headingDeg: Float,
        accuracyM: Float, alongM: Float, remainingM: Float, etaS: Int, flags: Int,
    ): ByteArray {
        val speed = if (speedMps.isFinite() && speedMps > 0f) speedMps else 0f
        var hdg = if (headingDeg.isFinite()) headingDeg % 360f else 0f
        if (hdg < 0f) hdg += 360f
        val along = if (alongM.isFinite() && alongM > 0f) alongM else 0f
        val remaining = if (remainingM.isFinite() && remainingM > 0f) remainingM else 0f
        val acc = if (accuracyM >= 0f && accuracyM < 255f) accuracyM.toInt() else 255
        val p = ByteBuffer.allocate(NAV_LEN).order(ByteOrder.LITTLE_ENDIAN)
        p.putInt(tMs).putInt(routeId)
        p.putInt(Math.round(lat * 1e7).toInt()).putInt(Math.round(lon * 1e7).toInt())
        p.putShort(clampU16(speed * 100f).toShort()).putShort((clampU16(hdg * 100f) % 36000).toShort())
        p.putFloat(along).putFloat(remaining).putInt(0)
        p.putInt(((etaS.toLong() and 0xFFFFFFFFL) % 86400L).toInt())
        p.put((flags or NAV_FREE).toByte()).put(acc.toByte())
        return frame(Packets.NAV_STATE, seq, p.array())
    }

    /** GUIDANCE, like rn_ffi_frame_guidance (road: first 32 bytes, upper-case ASCII). */
    fun guidance(seq: Int, tMs: Int, maneuver: Int, flags: Int, toTurnM: Int, remainingM: Long, etaS: Long, road: String): ByteArray {
        val man = if (maneuver in 0 until MANEUVER_COUNT) maneuver else MANEUVER_STRAIGHT
        val eta = if (etaS in 0 until 86400) etaS else UNKNOWN32
        val p = ByteBuffer.allocate(GUIDE_LEN).order(ByteOrder.LITTLE_ENDIAN)
        p.putInt(tMs).put(man.toByte()).put(flags.toByte()).putShort(toTurnM.toShort())
        p.putInt(remainingM.toInt()).putInt(eta.toInt())
        p.put(asciiUpper(road.toByteArray(Charsets.UTF_8).copyOf(minOf(ROAD_MAX, road.toByteArray(Charsets.UTF_8).size)), ROAD_MAX))
        return frame(Packets.GUIDANCE, seq, p.array())
    }

    /** ROUTE chunk frames for a whole line (18 points each), like rn_route_chunk_encode. */
    fun routeChunks(seqStart: Int, routeId: Int, packRouteId: Int, xs: FloatArray, ys: FloatArray, along: FloatArray): List<ByteArray> {
        val total = xs.size
        val totalM = if (total > 0) along[total - 1] else 0f
        val out = ArrayList<ByteArray>()
        var first = 0
        var seq = seqStart
        while (first < total) {
            val n = minOf(ROUTE_CHUNK_PTS, total - first)
            val p = ByteBuffer.allocate(17 + 12 * n).order(ByteOrder.LITTLE_ENDIAN)
            p.putInt(routeId).putInt(packRouteId).putShort(total.toShort()).putShort(first.toShort())
            p.put(n.toByte()).putFloat(totalM)
            for (i in first until first + n) p.putFloat(xs[i]).putFloat(ys[i]).putFloat(along[i])
            out.add(frame(Packets.ROUTE, seq and 0xFFFF, p.array()))
            seq++
            first += n
        }
        return out
    }

    private const val ROUTE_CHUNK_PTS = 18

    /** rn_text_ascii_upper into exactly [n] NUL-padded bytes. */
    fun asciiUpper(src: ByteArray, n: Int): ByteArray {
        val out = ByteArray(n)
        var k = 0
        var space = false
        for (b in src) {
            val c = b.toInt() and 0xFF
            if (c >= 0x80 || c < 0x20) continue
            if (c == ' '.code) { space = k > 0; continue }
            if (space && k + 2 < n + 1) { out[k++] = ' '.code.toByte() }
            space = false
            if (k >= n) break
            out[k++] = (if (c in 'a'.code..'z'.code) c - 32 else c).toByte()
        }
        return out
    }
}
