package dev.rotos.app.nav

/** Packet types and flags of the link protocol (navigation/include/rn/rn_protocol.h). */
object Packets {
    const val NAV_STATE = 0x01
    const val PHONE_STATUS = 0x02
    const val WEATHER = 0x03
    const val SETTINGS = 0x04
    const val FORECAST = 0x05
    const val CALL_STATE = 0x06
    const val PREP_STATUS = 0x07
    const val GUIDANCE = 0x08
    const val ROUTE = 0x0A
    const val GUIDE_ACTIVE = 0x01
    const val GUIDE_ARRIVED = 0x02
    const val MAP_BEGIN = 0x10
    const val MAP_CHUNK = 0x11
    const val MAP_END = 0x12
    const val PING = 0x20
    const val ACK = 0x21
    const val BUTTON = 0x22
    const val CALL_ACTION = 0x23
    const val DEVICE_STATUS = 0x24              // ROTOS -> phone: uptime, why it (re)started

    const val PREP_IDLE = 0
    const val PREP_ROUTE = 1
    const val PREP_BRIDGES = 2
    const val PREP_MAP = 3
    const val PREP_READY = 4
    const val PREP_FAILED = 5

    const val CALL_IDLE = 0
    const val CALL_RINGING = 1
    const val CALL_ACTIVE = 2
    const val CALL_F_REPLIED = 0x01
    const val CALL_F_MISSED = 0x02
    const val ACTION_REJECT_REPLY = 1
    const val ACTION_DISMISS = 2

    const val NAV_GPS_OK = 0x01
    const val NAV_OFF_ROUTE = 0x02
    const val NAV_REROUTING = 0x04
    const val NAV_NIGHT = 0x08
    const val NAV_ARRIVED = 0x10
    const val NAV_FREE = 0x20
    const val NAV_LINE = 0x40
    const val NAV_HEADING = 0x80

    const val NOTIFY_MESSAGE = 0
    const val NOTIFY_OTHER = 1
    const val STATUS_CHARGING = 0x01
    const val STATUS_NIGHT = 0x02
    /** Google usage near or at its cap: ROTOS warns, new maps may be refused. */
    const val STATUS_GOOGLE_CAP = 0x04

    const val ACK_OK = 0
    const val ACK_HAVE = 5
    const val ACK_BAD_CRC = 1
    const val ACK_TOO_BIG = 2
    const val ACK_BAD_STATE = 3
    const val ACK_STORAGE = 4
    const val ACK_NO_BASE = 6
}

/** A decoded inbound packet. */
data class RxPacket(val type: Int, val seq: Int, val payload: ByteArray, val len: Int)

data class Ack(val ackedType: Int, val status: Int, val ackedSeq: Int, val value: Long)

data class ButtonEvent(val button: Int, val event: Int)

/**
 * DEVICE_STATUS (ROTOS -> phone, every link-up): how long ROTOS has been up,
 * why it last started (esp_reset_reason_t), RN_DEVICE_RIDE_* flags and how
 * long it was off (-1 when not known yet).
 */
data class DeviceStatus(val uptimeMs: Long, val resetReason: Int, val flags: Int, val offS: Long) {
    companion object {
        const val RIDE_FOUND = 0x01
        const val RIDE_RESUMED = 0x02
        const val RIDE_CLOSED = 0x04

        /** 12-byte little-endian payload (docs/PROTOCOL.md); null if too short. Pure. */
        fun parse(p: ByteArray, len: Int): DeviceStatus? {
            if (len < 12 || p.size < 12) return null
            val b = java.nio.ByteBuffer.wrap(p, 0, 12).order(java.nio.ByteOrder.LITTLE_ENDIAN)
            val up = b.int.toLong() and 0xFFFFFFFFL
            val reason = b.get().toInt() and 0xFF
            val flags = b.get().toInt() and 0xFF
            b.short
            val off = b.int.toLong() and 0xFFFFFFFFL
            return DeviceStatus(up, reason, flags, if (off == 0xFFFFFFFFL) -1L else off)
        }

        /** esp_reset_reason_t in words. */
        fun reasonName(r: Int): String = when (r) {
            1 -> "power on"; 3 -> "software restart"; 4 -> "crash"; 5, 6, 7 -> "watchdog"
            9 -> "power dip (brownout)"; else -> "reason $r"
        }
    }
}

data class CallAction(val callId: Int, val action: Int)
