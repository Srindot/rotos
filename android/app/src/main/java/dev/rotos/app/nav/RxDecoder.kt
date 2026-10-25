package dev.rotos.app.nav

/**
 * Inbound byte stream -> packets, through the library's parser
 * (rn_ffi_rx_feed / rx_next / decode_ack / decode_button). Call on [NavThread].
 */
object RxDecoder {
    /** The library queues at most 16 packets; 160 bytes hold < 16 minimal (10 B) frames. */
    private const val SLICE = 160
    private const val MAX_SLICES = 4096

    fun feed(bytes: ByteArray): List<RxPacket> {
        val out = ArrayList<RxPacket>()
        val hdr = IntArray(2)
        for (s in 0 until MAX_SLICES) {
            val from = s * SLICE
            if (from >= bytes.size) break
            val part = bytes.copyOfRange(from, minOf(bytes.size, from + SLICE))
            NativeNav.rxFeed(part, part.size)
            drain(hdr, out)
        }
        return out
    }

    private fun drain(hdr: IntArray, out: MutableList<RxPacket>) {
        for (i in 0 until 32) {
            val payload = ByteArray(NativeNav.MAX_PAYLOAD)
            val n = NativeNav.rxNext(hdr, payload)
            if (n < 0) return
            out.add(RxPacket(hdr[0], hdr[1], payload, n))
        }
    }

    fun ack(p: RxPacket): Ack? {
        val o = IntArray(4)
        if (p.type != Packets.ACK || NativeNav.decodeAck(p.payload, p.len, o) < 0) return null
        return Ack(o[0], o[1], o[2], o[3].toLong() and 0xFFFFFFFFL)
    }

    fun button(p: RxPacket): ButtonEvent? {
        val o = IntArray(2)
        if (p.type != Packets.BUTTON || NativeNav.decodeButton(p.payload, p.len, o) < 0) return null
        return ButtonEvent(o[0], o[1])
    }

    fun callAction(p: RxPacket): CallAction? {
        val o = IntArray(2)
        if (p.type != Packets.CALL_ACTION || NativeNav.decodeCallAction(p.payload, p.len, o) < 0) return null
        return CallAction(o[0], o[1])
    }
}
