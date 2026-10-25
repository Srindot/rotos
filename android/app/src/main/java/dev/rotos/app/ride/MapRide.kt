package dev.rotos.app.ride

import dev.rotos.app.location.Fix
import dev.rotos.app.mirror.GuidanceBus
import dev.rotos.app.nav.Packets
import dev.rotos.app.quota.Purpose
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.Route
import dev.rotos.app.ui.UiModel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import java.time.LocalTime
import java.time.format.DateTimeFormatter
import kotlin.random.Random

/**
 * Google Maps navigates, ROTOS shows the map: each fix -> NAV_STATE (free
 * mode) at once, with Google's distance left and ETA, plus where the rider is
 * on the blue line. The line is a RouteLine sent as ROUTE packets and
 * followed here in Kotlin, so nothing on this path waits for NavThread.
 *
 * Google Maps taking another road (RerouteRules) fetches Google's new route
 * and sends just the new line (a fraction of a second); the old line stays
 * until ROTOS confirms the new one. Only if the new route leaves the map is a
 * new map built, in the background, while the line already shows.
 */
class MapRide(private val host: RideHost, private val send: FrameSender, private val ride: ActiveRide) {
    companion object {
        private const val LOG_MS = 10_000L
        private const val LINE_ACK_MS = 1_500L
        private const val LINE_TRIES = 3
        private val HHMM: DateTimeFormatter = DateTimeFormatter.ofPattern("HH:mm")
    }

    @Volatile private var follower: LineFollower? = null
    private var lastLogMs = 0L
    private var fixes = 0

    /** The map just reached ROTOS: put Google's route on it as the line (route rides). */
    suspend fun start(plan: Plan) {
        ride.pack = plan.pack
        ride.route = plan.source
        if (!ride.line) return
        val pts = plan.route.points
        val line = RouteLine.build(Random.nextInt(), plan.pack.routeId, pts, plan.route.maneuvers, plan.route.durationS,
            plan.pack.originX, plan.pack.originY) ?: return
        follower = LineFollower(line)                        // the pack holds the same route meanwhile
        if (!sendLine(line)) host.log("ROTOS did not confirm the line: it draws the map's own route")
        host.warmMap(pts)                                    // detours / reroutes: their map ready ahead
    }

    suspend fun onFix(scope: CoroutineScope, raw: Fix) {
        val fix = FixAhead.now(raw)
        val trusted = RerouteRules.trusted(fix.speedMps, fix.accuracyM)
        val f = follower
        val along = f?.let { val (x, y) = it.line.pxOf(LatLon(fix.lat, fix.lon)); it.follow(x, y, trusted) }
        position(fix, along)
        if (ride.lineLost && f != null) {
            ride.lineLost = false
            scope.launch { if (!sendLine(f.line)) ride.lineLost = true }      // after a reconnect
        }
        if (ride.busy || !trusted) return
        val here = LatLon(fix.lat, fix.lon)
        if (f != null && along != null) {
            val d = RerouteRules.decide(along, GuidanceBus.now.value, f.line.maneuvers, ride.watch, System.currentTimeMillis())
            ride.watch = d.watch
            d.note?.let { host.log(it) }
            if (d.reroute) scope.launch { reroute(scope, here) }
        } else if (!ride.line && RerouteRules.offMap(here, ride.coverage)) {
            scope.launch { refresh(here) }
        }
    }

    private suspend fun position(fix: Fix, along: Along?) {
        val g = GuidanceBus.now.value
        logRide(fix, along)
        // Google's arrival time; else our own estimate along the line
        val eta = g?.eta?.toSecondOfDay() ?: along?.let { LocalTime.now().plusSeconds(it.remainingS.toLong()).toSecondOfDay() } ?: 0
        val rerouting = g?.rerouting == true || ride.rerouting
        val flags = Packets.NAV_GPS_OK or
            (if (rerouting) Packets.NAV_REROUTING else 0) or
            (if (along != null) Packets.NAV_LINE else 0) or
            (if (fix.headingDeg >= 0f && fix.speedMps >= 0f) Packets.NAV_HEADING else 0) or
            (if (Dashboard.isNight(fix.lat, fix.lon, fix.unixS)) Packets.NAV_NIGHT else 0)
        send.position(fix, ride.routeId, flags, along, g?.remainingM?.toFloat() ?: along?.remainingM ?: 0f, eta)
        fixes++
        val snap = NavSnapshot(fixes, along?.alongM ?: 0f, (g?.remainingM ?: 0).toFloat(), 0, Packets.NAV_GPS_OK,
            g?.eta?.format(HHMM) ?: "—", maxOf(fix.speedMps, 0f))
        host.updateStatus { it.copy(nav = snap) }
        host.rideNotice { g?.let { "Google Maps · ${UiModel.describe(it)}" } ?: "ROTOS map on · navigate in Google Maps" }
    }

    private fun logRide(fix: Fix, along: Along?) {
        val now = System.currentTimeMillis()
        if (now - lastLogMs < LOG_MS) return
        lastLogMs = now
        val g = GuidanceBus.now.value
        host.log("ride: %.5f,%.5f ±%.0f m %.0f km/h%s | Google: %s | link %s".format(fix.lat, fix.lon, fix.accuracyM,
            maxOf(fix.speedMps, 0f) * 3.6f,
            along?.let { " | line %.1f/%.1f km left".format(it.alongM / 1000f, it.remainingM / 1000f) } ?: "",
            g?.let { UiModel.describe(it) } ?: "-", send.link.state.value))
    }

    /** Sends [line] until ROTOS confirms it (ACK value = line id). */
    private suspend fun sendLine(line: RouteLine): Boolean {
        while (host.lineAcks.tryReceive().isSuccess) { /* drop stale confirmations */ }
        for (attempt in 1..LINE_TRIES) {
            val t0 = System.currentTimeMillis()
            send.routeLine(line)
            val ok = withTimeoutOrNull(LINE_ACK_MS) {
                var got = false
                while (!got) got = host.lineAcks.receive().value.toInt() == line.id
                true
            } ?: false
            if (ok) {
                host.log("line on ROTOS: %d points, %.1f km, in %d ms".format(line.size, line.totalM / 1000f,
                    System.currentTimeMillis() - t0))
                return true
            }
        }
        return false
    }

    /**
     * Google took another road: its new route as the new line, at once. The old
     * line stays if anything fails, and the next try comes within seconds.
     */
    private suspend fun reroute(scope: CoroutineScope, here: LatLon) {
        ride.rerouting = true
        ride.watch = RerouteWatch()
        try {
            host.log("route changed: new Google route from %.5f,%.5f".format(here.lat, here.lon))
            val route = host.findRoute(ride.cfg.copy(origin = here), Purpose.AUTO).getOrElse {
                host.log("reroute: ${it.message?.take(100)}: keeping the line, trying again shortly")
                ride.failed()
                return
            }
            val pack = ride.pack ?: return
            val (pts, turns) = RouteLine.pointsOf(route)
            val line = RouteLine.build(Random.nextInt(), pack.routeId, pts, turns, route.durationS,
                pack.originX, pack.originY) ?: return
            if (!sendLine(line)) {
                host.log("reroute: ROTOS did not confirm the new line; trying again shortly")
                ride.failed()
                return
            }
            follower = LineFollower(line)
            ride.route = route
            ride.line = true
            if (!covered(pts)) scope.launch { mapFor(route, here) }
            host.warmMap(pts)
        } finally {
            ride.rerouting = false
            host.updateStatus { it.copy(phase = "riding", error = "") }
        }
    }

    /** Does the map ROTOS holds show the new route and the roads around it? */
    private fun covered(pts: List<LatLon>): Boolean = RerouteRules.covers(pts, ride.coverage)

    /** A map for the new route, in the background; the line is already on ROTOS. */
    private suspend fun mapFor(route: Route, here: LatLon) {
        if (ride.mapping) return
        ride.mapping = true
        try {
            host.log("new route leaves the map: building its map in the background")
            val plan = host.replan(ride.cfg.copy(origin = here), Purpose.AUTO, route) ?: return
            if (!host.sendMap(plan.pack)) return
            send.prep(Packets.PREP_IDLE)
            ride.routeId = plan.pack.routeId
            ride.coverage = plan.route.points
            start(plan)                                      // the line again, in the new map's pixels
        } finally {
            ride.mapping = false
            host.updateStatus { it.copy(phase = "riding", error = "") }
        }
    }

    /** The rider left ROTOS's map (area rides): build the area around them and send it. */
    private suspend fun refresh(here: LatLon) {
        ride.refreshing = true
        try {
            host.log("left the map: building the area around %.5f,%.5f".format(here.lat, here.lon))
            val plan = host.replan(ride.cfg.copy(origin = here, destination = here, area = true), Purpose.AUTO)
            if (plan == null || !host.sendMap(plan.pack)) {
                ride.failed()
                return
            }
            send.prep(Packets.PREP_IDLE)
            ride.routeId = plan.pack.routeId
            ride.coverage = plan.route.points
            ride.pack = plan.pack
            host.warmMap(plan.route.points)
        } finally {
            ride.refreshing = false
            host.updateStatus { it.copy(phase = "riding", error = "") }
        }
    }
}

/**
 * GPS fixes reach the app ~0.3-1 s after they were taken: move the position
 * on by speed × age along the course, so ROTOS shows where the rider is now,
 * not where they were. Pure.
 */
object FixAhead {
    private const val MAX_AGE_MS = 1_500L
    private const val MIN_SPEED = 1.5f

    fun now(f: Fix, nowMs: Long = android.os.SystemClock.elapsedRealtime()): Fix = ahead(f, nowMs)

    fun ahead(f: Fix, nowMs: Long): Fix {
        val age = ((nowMs and 0xFFFFFFFFL) - (f.tMs.toLong() and 0xFFFFFFFFL)).coerceIn(0L, MAX_AGE_MS)
        if (f.speedMps < MIN_SPEED || f.headingDeg < 0f || age == 0L) return f
        val d = f.speedMps * age / 1000.0
        val h = Math.toRadians(f.headingDeg.toDouble())
        val dLat = d * Math.cos(h) / 111_320.0
        val dLon = d * Math.sin(h) / (111_320.0 * Math.cos(Math.toRadians(f.lat)))
        return f.copy(lat = f.lat + dLat, lon = f.lon + dLon, tMs = (nowMs and 0xFFFFFFFFL).toInt())
    }
}
