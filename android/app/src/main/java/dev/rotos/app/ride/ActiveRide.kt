package dev.rotos.app.ride

import dev.rotos.app.map.PackInfo
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.Route

/**
 * The running ride's state, shared by its loops; one ride at a time, a new
 * ride gets a new instance. Written from the main thread and NavThread.
 */
class ActiveRide(val cfg: RideConfig) {
    companion object {
        /** After a failed replan, no new one for this long (no billed request per fix). */
        const val REPLAN_BACKOFF_MS = 8_000L
    }

    /** The map pack ROTOS holds now (its origin places the line). */
    @Volatile var pack: PackInfo? = null
    /** The route the line shows (Google's, as decoded), for building its map later. */
    @Volatile var route: Route? = null
    /** Building a map for the current route in the background (the line is already on ROTOS). */
    @Volatile var mapping = false
    /** The link came back (ROTOS may have restarted): the line lives in its RAM, send it again. */
    @Volatile var lineLost = false

    /** Route id of the map pack ROTOS holds. */
    @Volatile var routeId = 0
    /** ROTOS draws Google's route (the phone follows it with the engine). */
    @Volatile var line = false
    /** The map reached ROTOS: stopping now shows the ride summary there. */
    @Volatile var reached = false
    /** Points the current map pack was built around (route or area sweep). */
    @Volatile var coverage: List<LatLon> = emptyList()
    /** A new route is being planned and sent. */
    @Volatile var rerouting = false
    /** A new area map is being built and sent. */
    @Volatile var refreshing = false
    @Volatile var failedMs = 0L
    var watch = RerouteWatch()

    val busy: Boolean get() = rerouting || refreshing || System.currentTimeMillis() - failedMs < REPLAN_BACKOFF_MS

    fun failed() {
        failedMs = System.currentTimeMillis()
    }
}
