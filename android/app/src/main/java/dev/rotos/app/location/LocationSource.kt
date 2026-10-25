package dev.rotos.app.location

import kotlinx.coroutines.flow.Flow

/** One position fix, as the engine wants it (rn_engine.h RnFix). */
data class Fix(
    /** Monotonic ms (u32 bits); the engine derives speed from it. */
    val tMs: Int,
    /** Wall clock, UTC seconds (sun position / ETA). */
    val unixS: Double,
    val lat: Double,
    val lon: Double,
    /** < 0 = unknown */
    val speedMps: Float,
    /** < 0 = unknown */
    val headingDeg: Float,
    val accuracyM: Float,
)

/** Something that produces fixes at about 1 Hz until cancelled (or the demo ride ends). */
interface LocationSource {
    val name: String
    fun fixes(): Flow<Fix>
}
