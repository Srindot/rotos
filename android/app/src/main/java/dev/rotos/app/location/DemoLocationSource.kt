package dev.rotos.app.location

import android.os.SystemClock
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow

/**
 * A rider on the loaded route, from the library's ride simulator
 * (rn_ffi_sim_*: realistic acceleration, turns, a red light, GPS noise).
 * One fix per simulated second, delivered every 1/speedup real seconds;
 * after arrival it keeps reporting the (stopped) end point for a few seconds
 * so the engine sees the arrival.
 */
class DemoLocationSource(
    private val speedup: Float = 1f,
    private val seed: Int = 7,
    private val cruiseMps: Float = 12f,
    private val sigmaM: Float = 3f,
) : LocationSource {
    override val name = "demo"

    private companion object {
        const val TAIL_FIXES = 8
        const val MAX_FIXES = 6 * 3600
    }

    override fun fixes(): Flow<Fix> = flow {
        val st = NavThread.run { NativeNav.simStart(seed, cruiseMps) }
        check(st >= 0) { "sim_start failed (rn_status $st)" }
        val t0 = SystemClock.elapsedRealtime()
        val unix0 = System.currentTimeMillis() / 1000.0
        val periodMs = (1000f / speedup.coerceIn(0.1f, 50f)).toLong()
        var tail = 0
        for (i in 1..MAX_FIXES) {
            delay(maxOf(0L, t0 + i * periodMs - SystemClock.elapsedRealtime()))   // absolute cadence, no drift
            val riding = NavThread.run { NativeNav.simStep(1f) }
            val ll = DoubleArray(2)
            val f = FloatArray(3)
            NavThread.run { NativeNav.simFix(sigmaM, ll, f) }
            emit(Fix((t0 + i * 1000L).toInt(), unix0 + i, ll[0], ll[1], f[0], f[1], f[2]))
            if (riding <= 0 && ++tail >= TAIL_FIXES) break
        }
    }
}
