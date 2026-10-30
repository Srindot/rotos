package dev.rotos.app.ride

import dev.rotos.app.mirror.GuidanceBus
import dev.rotos.app.route.GoogleTarget
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import java.time.LocalTime

/**
 * Google Maps navigating (GuidanceBus) -> GUIDANCE to ROTOS on every change
 * and every 2 s (ROTOS drops it after 8 s of silence): its turn screen, or
 * the HUD over its map during a ride. One inactive GUIDANCE when Google Maps
 * stops. Also remembers the roads Google announced, for picking its route.
 */
class GuidanceMirror(
    private val send: FrameSender,
    /** True after the rider stopped the ride: Google's turns stay off ROTOS until Google stops navigating. */
    private val hidden: () -> Boolean,
    private val log: (String) -> Unit,
) {
    companion object {
        private const val GUIDE_MS = 2_000L
        private const val ROADS_KEPT = 5
        /** How long [target] waits for Google's first summary after a share. */
        private const val TARGET_WAIT_MS = 8_000L
    }

    /** Road names Google Maps announced recently, newest first. */
    @Volatile var roads: List<String> = emptyList()
        private set

    suspend fun run() = coroutineScope {
        launch { watchRoads() }
        var shown = false
        while (isActive) {
            val g = GuidanceBus.now.value?.takeUnless { hidden() }
            if (g != null) {
                send.guidance(g)
                shown = true
            } else if (shown) {
                send.guidanceOff()
                shown = false
            }
            withTimeoutOrNull(GUIDE_MS) { GuidanceBus.now.drop(1).first() }
        }
    }

    private suspend fun watchRoads() {
        GuidanceBus.now.collect { g ->
            if (g == null) return@collect
            if (g.rerouting) log("Google Maps is rerouting")
            else log("Google Maps: ${g.road.ifEmpty { "turn" }} in ${g.toTurnM ?: "?"} m")
            if (g.road.isNotEmpty() && roads.firstOrNull() != g.road) {
                roads = (listOf(g.road) + roads.filter { it != g.road }).take(ROADS_KEPT)
            }
        }
    }

    /**
     * What Google Maps is navigating, for picking the matching route. Waits up
     * to [TARGET_WAIT_MS] for its first real summary (a shared place starts
     * Google navigation and planning at the same moment).
     */
    /** A navigation ended: its roads must not steer the next trip's route choice. */
    fun forgetRoads() {
        roads = emptyList()
    }

    suspend fun target(waitMs: Long = TARGET_WAIT_MS, fresh: Boolean = false): GoogleTarget? {
        val before = GuidanceBus.now.value
        val g = withTimeoutOrNull(waitMs) {
            // fresh: a summary that differs from the one showing now (the new trip, not the old one)
            GuidanceBus.now.first {
                it != null && !it.rerouting && it.remainingM != null && (!fresh || it.remainingM != before?.remainingM)
            }
        } ?: if (fresh) null else GuidanceBus.now.value
        if (g == null) {
            log("route: no Google Maps summary yet, using the first route")
            return null
        }
        val secs = g.eta?.let { e ->
            val d = e.toSecondOfDay() - LocalTime.now().toSecondOfDay()
            if (d < 0) d + 86_400 else d
        }
        return GoogleTarget(g.remainingM, secs, roads)
    }
}
