package dev.rotos.app.nav

import org.junit.Assert.assertEquals
import org.junit.Test

/** Expected bytes come from the C library (rn_ffi_frame_position / rn_ffi_frame_guidance). */
class FrameCodecTest {
    private fun hex(b: ByteArray) = b.joinToString("") { "%02x".format(it) }

    @Test fun positionMatchesC() {
        val a = FrameCodec.position(513, 123456, 0x89462d6c.toInt(), 17.4475512, 78.5046789, 11.7f, 271.4f, 6f,
            1234.5f, 20876.25f, 61234, 0x01 or 0x40 or 0x80)
        assertEquals("a55a02010102260040e201006c2d4689f848660a05ddca2e9204046a00509a448018a3460000000032ef0000e106a25a", hex(a))
        val b = FrameCodec.position(65535, 4000000000L.toInt(), 0, -33.8688197, 151.2092955, -1f, -90f, 300f,
            Float.NaN, -5f, 86399, 0x01)
        assertEquals("a55a0201ffff260000286bee000000003b07d0eb1bb5205a000078690000000000000000000000007f51010021ffaf18", hex(b))
    }

    @Test fun guidanceMatchesC() {
        val g = FrameCodec.guidance(7, 99000, 4, 1, 180, 22000, 37800, "W Marredpally Rd  ✓ ")
        assertEquals("a55a020807003000b88201000401b400f0550000a893000057204d415252454450414c4c5920524400000000000000000000000000000000f0d8", hex(g))
        val off = FrameCodec.guidance(8, 1, 0, 0, 0xFFFF, 0xFFFFFFFFL, 0xFFFFFFFFL, "")
        assertEquals("a55a020808003000010000000000ffffffffffffffffffff0000000000000000000000000000000000000000000000000000000000000000bf75", hex(off))
    }
}
