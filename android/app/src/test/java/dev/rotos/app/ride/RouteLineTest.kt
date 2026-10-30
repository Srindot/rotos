package dev.rotos.app.ride

import dev.rotos.app.location.Fix
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.Polyline
import dev.rotos.app.route.Route
import dev.rotos.app.route.RouteStep
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RouteLineTest {
    // a straight 2 km road east of IIIT Hyderabad, then 1 km north
    private val pts = (0..200).map { LatLon(17.4455, 78.3489 + it * 0.0000943) } +
        (1..100).map { LatLon(17.4455 + it * 0.00009, 78.3489 + 200 * 0.0000943) }
    private val origin = RouteLine.worldX(78.34) to RouteLine.worldY(17.46)
    private fun line() = RouteLine.build(7, 99, pts, emptyList(), 600f, origin.first.toInt(), origin.second.toInt())!!

    @Test fun polylineRoundTrips() {
        val back = Polyline.decode(Polyline.encode(pts))
        assertEquals(pts.size, back.size)
        assertEquals(pts.last().lat, back.last().lat, 1e-5)
        assertEquals(pts.last().lon, back.last().lon, 1e-5)
    }

    @Test fun lineIsSimplifiedButKeepsItsLength() {
        val l = line()
        assertTrue(l.size in 3..10)                      // two straight legs: a handful of points
        assertEquals(3000f, l.totalM, 60f)              // ~2 km + ~1 km
    }

    @Test fun routeStepsJoinIntoOneLine() {
        val r = Route(listOf(RouteStep(Polyline.encode(pts.take(101)), "STRAIGHT", "Head east on Road No 1", 1000),
            RouteStep(Polyline.encode(pts.drop(100)), "TURN_LEFT", "Turn left onto Road No 2", 2000)), 600f, 3000, "google")
        val (all, turns) = RouteLine.pointsOf(r)
        assertEquals(pts.size, all.size)                 // the shared joint once
        assertEquals(listOf(0f, 1000f), turns.map { it.alongM })
        assertEquals("Road No 2", turns[1].road)
    }

    @Test fun followerTracksAlongAndNoticesLeavingTheLine() {
        val l = line()
        val f = LineFollower(l)
        val (x, y) = l.pxOf(pts[100])
        val a = f.follow(x, y, trusted = true)
        assertEquals(1000f, a.alongM, 40f)
        assertFalse(a.wantsReroute)
        val (ox, oy) = l.pxOf(LatLon(17.4455 - 0.0009, 78.3489 + 120 * 0.0000943))   // ~100 m south of the road
        repeat(LineFollower.OFF_FIXES - 1) { assertFalse(f.follow(ox, oy, trusted = true).wantsReroute) }
        assertTrue(f.follow(ox, oy, trusted = true).wantsReroute)
        assertFalse(LineFollower(l).follow(ox, oy, trusted = false).wantsReroute)  // untrusted fixes never count
    }

    @Test fun fixesAreMovedToNow() {
        val f = Fix(1_000, 0.0, 17.4455, 78.3489, 10f, 90f, 5f)          // 10 m/s east, taken at t=1000 ms
        val g = FixAhead.ahead(f, 1_500L)
        val east = (g.lon - f.lon) * 111_320.0 * Math.cos(Math.toRadians(17.4455))
        assertEquals(5.0, east, 0.2)                                          // 0.5 s x 10 m/s
        val still = f.copy(speedMps = 0.5f)
        assertEquals(still, FixAhead.ahead(still, 1_500L))                    // standing: unchanged
    }

    @Test fun aUTurnOnADividedRoadIsNotSkipped() {
        // 1 km north, U-turn across a 15 m median, 1 km back south
        val out = (0..100).map { LatLon(17.4400 + it * 0.00009, 78.3500) }
        val back = (0..100).map { LatLon(17.4490 - it * 0.00009, 78.35014) }
        val l = RouteLine.build(8, 99, out + back, emptyList(), 300f, origin.first.toInt(), origin.second.toInt())!!
        val f = LineFollower(l)
        // riding north, 300 m before the U-turn, the fix drifting 9 m toward the far carriageway
        repeat(3) { k ->
            val (x, y) = l.pxOf(LatLon(17.4460 + k * 0.00005, 78.35008))
            val a = f.follow(x, y, trusted = true, courseDeg = 0f)
            assertTrue("along ${a.alongM} must stay on the way out", a.alongM < 1_000f)
        }
    }
}
