package dev.rotos.app.route

import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class RouteCutTest {
    // two steps east along 17.44 N: 2 km then 8 km (~0.0943 deg of longitude per 10 km)
    private val a = (0..20).map { LatLon(17.44, 78.35 + it * 0.000943) }
    private val b = (20..100).map { LatLon(17.44, 78.35 + it * 0.000943) }
    private val route = Route(
        steps = listOf(RouteStep(Polyline.encode(a), "", "Head east", 2_000), RouteStep(Polyline.encode(b), "", "Continue", 8_000)),
        durationS = 1_000f, distanceM = 10_000, source = "t")

    @Test fun keepsTheFirstKilometresOnly() {
        val cut = RouteCut.first(route, 3_000)
        assertEquals(2, cut.steps.size)                                  // the first step whole, the second cut
        assertEquals(3_000, cut.distanceM)
        assertEquals(300f, cut.durationS, 1f)
        val end = Polyline.decode(cut.steps[1].encodedPolyline).last()
        val km = AreaRoute.distanceToPath(LatLon(17.44, 78.35), listOf(end)) / 1000
        assertTrue("ends ~3 km in, not $km", km in 2.9..3.2)
    }

    @Test fun aShortRouteStaysWhole() = assertSame(route, RouteCut.first(route, 20_000))
}
