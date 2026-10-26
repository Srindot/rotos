package dev.rotos.app.ride

import dev.rotos.app.map.PackInfo
import dev.rotos.app.quota.Purpose
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.Route

/**
 * The running ride's state, shared by its loops; one ride at a time, a new
 * ride gets a new instance. Written from the main thread and NavThread.
 */
class ActiveRide(val cfg: RideConfig, val purpose: Purpose = Purpose.USER) {
    companion object {
        /** After a failed replan, no new one for this long (no billed request per fix). */
        const val REPLAN_BACKOFF_MS = 8_000L
        /** A reroute this soon after the last one is a repeat: the disagreement did not go away. */
        const val REPEAT_MS = 120_000L
        const val HOLD_FIRST_MS = 30_000L
        const val HOLD_MAX_MS = 600_000L
        /** Automatic reroutes per hour at most (each is a billed Routes call). */
        const val MAX_PER_HOUR = 20
    }

    private var lastRerouteMs = 0L
    private var streak = 0
    private val recent = ArrayDeque<Long>()
    /** No automatic reroute before this (repeats back off: 30 s, 1, 2, 4 ... 10 min). */
    @Volatile var holdUntilMs = 0L
    /** The hour's budget of automatic reroutes is spent until then (nothing reroutes before). */
    @Volatile var capUntilMs = 0L

    /**
     * A reroute was made; [same] = Google's route was the one already shown.
     * A repeat within [REPEAT_MS] (or a no-change) doubles the hold, so a
     * disagreement that no reroute fixes (Google navigates a route the Routes
     * API does not offer, another destination) never burns the daily cap.
     */
    @Synchronized
    fun rerouted(same: Boolean, now: Long = System.currentTimeMillis()) {
        // a repeat: within REPEAT_MS of the last reroute *or of the end of its hold* (the disagreement
        // that comes straight back after a hold is the same one: the hold must keep growing)
        val repeat = same || (lastRerouteMs != 0L && now - maxOf(lastRerouteMs, holdUntilMs) < REPEAT_MS)
        streak = if (repeat) streak + 1 else 0
        lastRerouteMs = now
        recent.addLast(now)
        while (recent.isNotEmpty() && now - recent.first() >= 3_600_000L) recent.removeFirst()
        holdUntilMs = now + if (streak == 0) 0L else minOf(HOLD_FIRST_MS shl minOf(streak - 1, 10), HOLD_MAX_MS)
        capUntilMs = if (recent.size >= MAX_PER_HOUR) recent.first() + 3_600_000L else 0L
    }

    /**
     * May an automatic reroute run now? Never past the hour's cap. A real
     * wrong turn ([offLine]: the rider left ROTOS's line) waits at most 30 s
     * of a hold; Google merely disagreeing waits out all of it.
     */
    @Synchronized
    fun mayReroute(offLine: Boolean, now: Long = System.currentTimeMillis()): Boolean {
        if (now < capUntilMs) return false
        val until = if (offLine) minOf(holdUntilMs, lastRerouteMs + HOLD_FIRST_MS) else holdUntilMs
        return now >= until
    }

    /** Seconds until automatic reroutes may run again. */
    fun holdS(): Long = ((maxOf(holdUntilMs, capUntilMs) - System.currentTimeMillis()).coerceAtLeast(0L) + 999L) / 1000L

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
    /** The route was too long for one map: riding with the area map around the rider instead. */
    @Volatile var areaFallback = false
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
