package dev.rotos.app.nav

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

/** Bytes as the C encoder writes them (core/tests/test_protocol.c test_device_status). */
class DeviceStatusTest {
    @Test fun parsesWhatRotosSends() {
        val p = byteArrayOf(0x40, 0xE2.toByte(), 0x01, 0x00, 9, 3, 0, 0, 38, 0, 0, 0)
        assertEquals(DeviceStatus(123456L, 9, 3, 38L), DeviceStatus.parse(p, 12))
        val unknown = byteArrayOf(0x10, 0x27, 0, 0, 1, 1, 0, 0, -1, -1, -1, -1)
        assertEquals(-1L, DeviceStatus.parse(unknown, 12)!!.offS)
        assertNull(DeviceStatus.parse(p, 11))
    }
}
