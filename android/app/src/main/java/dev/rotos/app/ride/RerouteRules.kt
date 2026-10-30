package dev.rotos.app.ride

import dev.rotos.app.mirror.Guidance
import dev.rotos.app.route.AreaRoute
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.ManeuverInfo
import dev.rotos.app.route.RoadNames
import dev.rotos.app.route.Route

/** Where the rider is along the line ROTOS draws (the C engine's map matching). */
data class Along(val alongM: Float, val remainingM: Float, val remainingS: Int, val wantsReroute: Boolean)

/**
 * When each disagreement with Google Maps started (0 = none right now), and
 * whether Google Maps was seen "Rerouting…" (its new route is then due).
 */
data class RerouteWatch(val distanceSince: Long = 0L, val roadSince: Long = 0L, val googleRerouting: Boolean = false)

/** Why a reroute is asked for. OFF_LINE and GOOGLE are urgent: the line on ROTOS is wrong now. */
enum class RerouteWhy { NONE, OFF_LINE, GOOGLE, ROAD, DISTANCE }

/** [reroute]: ask for a new route now ([why]). [note]: something worth logging, once. */
data class RerouteDecision(
    val reroute: Boolean, val watch: RerouteWatch, val note: String? = null, val why: RerouteWhy = RerouteWhy.NONE,
) {
    val urgent: Boolean get() = why == RerouteWhy.OFF_LINE || why == RerouteWhy.GOOGLE
}

/**
 * When the line on ROTOS no longer is Google's route, as pure functions
 * (the caller keeps the [RerouteWatch] between fixes and owns the clock):
 *   1. the engine's off-route hysteresis says so ([Along.wantsReroute]);
 *   2. Google announces a road that is nowhere on our line from 300 m behind
 *      to 4 km ahead, for [MISMATCH_MS];
 *   3. Google's distance left differs from ours by more than
 *      max(800 m, 20 %), for [DISTANCE_MS] (Google rerouted on its own);
 *   4. Google Maps showed "Rerouting…" and now navigates again: its new
 *      route is there, take it at once.
 * Every reroute is a billed Routes call, so the caller also keeps a backoff
 * and UsageGuard keeps 30 s between automatic calls.
 */
object RerouteRules {
    const val MISMATCH_MS = 15_000L
    /** Distances disagree this long: Google's route changed (rounding never makes 800 m). */
    const val DISTANCE_MS = 6_000L
    /** The corridor reaches ~620 m from the line; refresh the map before its edge. */
    const val EDGE_M = 450.0
    /**
     * A route ride's map reaches only ~410 m from its route (ROUTE_BUFFER_PX at
     * zoom 16) and the screen shows ~280 m to each side of the rider: a new
     * route further than this from the map's route needs its own map.
     */
    const val LINE_COVER_M = 120.0
    private const val THIN_M = 25.0
    /** Within this of the shown line all along = the same road (GPS / polyline wobble). */
    private const val SAME_M = 30.0
    private const val BEHIND_M = 300f
    private const val AHEAD_M = 4_000f
    private const val MIN_DIFF_M = 800f
    private const val DIFF_SHARE = 0.2f
    /** Below this speed, or with a worse GPS fix, nobody leaves a route: no decisions. */
    const val MIN_SPEED_MPS = 2.0f
    const val MAX_ACCURACY_M = 40f

    /**
     * A fix good enough to judge the route by: moving and well located.
     * Standing still indoors, GPS wanders and Google Maps announces nearby
     * streets, which would otherwise look like "Google took another route".
     */
    fun trusted(speedMps: Float, accuracyM: Float): Boolean =
        speedMps >= MIN_SPEED_MPS && accuracyM in 0f..MAX_ACCURACY_M

    fun decide(a: Along, g: Guidance?, line: List<ManeuverInfo>?, w: RerouteWatch, now: Long): RerouteDecision {
        if (a.wantsReroute) return RerouteDecision(true, w, why = RerouteWhy.OFF_LINE)
        if (g?.rerouting == true) return RerouteDecision(false, w.copy(googleRerouting = true))
        if (w.googleRerouting && g != null) return RerouteDecision(true, RerouteWatch(), "Google Maps picked a new route", RerouteWhy.GOOGLE)
        val (roadSince, note) = roadWatch(a, g, line, w.roadSince, now)
        if (roadSince != 0L && now - roadSince >= MISMATCH_MS) {
            return RerouteDecision(true, w.copy(roadSince = roadSince), note, RerouteWhy.ROAD)
        }
        val distSince = distanceWatch(a, g, w.distanceSince, now)
        val reroute = distSince != 0L && now - distSince >= DISTANCE_MS
        return RerouteDecision(reroute, RerouteWatch(distSince, roadSince), note, if (reroute) RerouteWhy.DISTANCE else RerouteWhy.NONE)
    }

    /**
     * Is [new] the route ROTOS already shows ([old])? Only if it runs on the same
     * road all along: every point of it within [SAME_M] of the old line. (A
     * similar length to the same destination is not enough: a parallel road
     * often is just as long - and then the rider would never get its line.)
     */
    fun sameRoute(old: Route?, new: Route): Boolean {
        if (old == null) return false
        val path = thin(RouteLine.pointsOf(old).first)
        if (path.size < 2) return false
        return dense(thin(RouteLine.pointsOf(new).first)).all { AreaRoute.distanceToPath(it, path) <= SAME_M }
    }

    /** Does the map built around [coverage] show all of [pts] with the roads around them? */
    fun covers(pts: List<LatLon>, coverage: List<LatLon>): Boolean {
        if (coverage.isEmpty()) return false
        val cover = thin(coverage)                    // ~25 m apart: thousands x thousands would take seconds
        return dense(thin(pts)).all { AreaRoute.distanceToPath(it, cover) <= LINE_COVER_M }
    }

    /** Points every [THIN_M] along the polyline: a straight bypass is checked in its middle too. */
    fun dense(pts: List<LatLon>): List<LatLon> {
        if (pts.size < 2) return pts
        val out = ArrayList<LatLon>(pts.size * 2)
        for (i in 0 until pts.size - 1) {
            val a = pts[i]
            val b = pts[i + 1]
            val n = maxOf(1, (AreaRoute.distanceToPath(a, listOf(b)) / THIN_M).toInt())
            for (k in 0 until n) out.add(LatLon(a.lat + (b.lat - a.lat) * k / n, a.lon + (b.lon - a.lon) * k / n))
        }
        out.add(pts.last())
        return out
    }

    /** Points at least [THIN_M] apart (first and last kept): the shape within a few metres. */
    fun thin(pts: List<LatLon>): List<LatLon> {
        if (pts.size < 3) return pts
        val out = ArrayList<LatLon>(pts.size / 4 + 2)
        out.add(pts.first())
        for (i in 1 until pts.size - 1) {
            if (AreaRoute.distanceToPath(pts[i], listOf(out.last())) >= THIN_M) out.add(pts[i])
        }
        out.add(pts.last())
        return out
    }

    /** Further than [EDGE_M] from every point the map was built around. */
    fun offMap(here: LatLon, coverage: List<LatLon>): Boolean =
        coverage.isNotEmpty() && AreaRoute.distanceToPath(here, coverage) > EDGE_M

    /** (since, note): when Google's announced road started to be missing from our line. */
    private fun roadWatch(a: Along, g: Guidance?, line: List<ManeuverInfo>?, since: Long, now: Long): Pair<Long, String?> {
        if (g == null || g.rerouting || g.road.isEmpty() || line == null) return 0L to null
        val near = line.filter { it.alongM in (a.alongM - BEHIND_M)..(a.alongM + AHEAD_M) }.map { it.road }
        if (near.isEmpty() || near.any { RoadNames.similar(it, g.road) }) return 0L to null
        if (since != 0L) return since to null
        return now to "Google says '${g.road}', not on ROTOS's line ahead (${near.take(4)})"
    }

    /** When Google's distance left started to disagree with ours. */
    private fun distanceWatch(a: Along, g: Guidance?, since: Long, now: Long): Long {
        val google = g?.remainingM ?: return 0L
        val diff = Math.abs(google - a.remainingM)
        if (diff < maxOf(MIN_DIFF_M, DIFF_SHARE * a.remainingM)) return 0L
        return if (since == 0L) now else since
    }
}
