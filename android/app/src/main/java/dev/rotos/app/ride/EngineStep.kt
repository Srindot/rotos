package dev.rotos.app.ride

import dev.rotos.app.location.Fix
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import java.util.TimeZone

/**
 * One GPS fix through the C navigation engine (map matching, ETA, off-route
 * hysteresis), on NavThread. [frame]: the NAV_STATE it built for ROTOS.
 */
class EngineStep(val seq: Int, val frame: ByteArray, val along: Along, val flags: Int) {
    companion object {
        /** [seq] numbers the NAV_STATE frame (0 when it is not sent). Null on failure. */
        suspend fun run(seq: Int, fix: Fix): EngineStep? {
            val off = TimeZone.getDefault().getOffset(System.currentTimeMillis()) / 1000
            val f = FloatArray(2)
            val i = IntArray(3)
            val buf = ByteArray(NativeNav.MAX_FRAME)
            val n = NavThread.run {
                val len = NativeNav.engineFix(seq, fix.tMs, fix.unixS, off, fix.lat, fix.lon, fix.speedMps,
                    fix.headingDeg, fix.accuracyM, buf)
                if (len > 0 && NativeNav.engineState(f, i) >= 0) len else -1
            }
            if (n <= 0) return null
            return EngineStep(seq, buf.copyOf(n), Along(f[0], f[1], i[0], i[2] == 1), i[1])
        }

        suspend fun reset() = NavThread.run { NativeNav.engineReset() }
    }
}
