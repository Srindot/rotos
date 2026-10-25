package dev.rotos.app.route

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RouteMatcherTest {
    private fun route(km: Double, min: Int, vararg roads: String) = Route(
        steps = roads.map { RouteStep("??", "TURN_LEFT", "Turn left onto $it", 100) },
        durationS = min * 60f, distanceM = (km * 1000).toInt(), source = "osrm",
    )

    @Test fun googleAndOsmSpellings() {
        assertTrue(RoadNames.similar("W Jubilee Hills Rd", "West Jubilee Hills Road"))
        assertTrue(RoadNames.similar("St No 7", "Street Number 7"))
        assertTrue(RoadNames.similar("Old Mumbai Hwy", "Old Mumbai Highway"))
        assertFalse(RoadNames.similar("St No 7", "Street Number 9"))
        assertFalse(RoadNames.similar("Gachibowli Rd", "Road No 36"))
    }

    @Test fun picksTheRouteGoogleIsOn() {
        val a = route(22.5, 38, "Paradise Road", "Begumpet Road")
        val b = route(22.0, 36, "West Jubilee Hills Road", "Street Number 7")
        val c = route(25.0, 41, "Outer Ring Road")
        val t = GoogleTarget(22_000, 36 * 60, listOf("W Jubilee Hills Rd", "St No 7"))
        assertEquals(1, RouteMatcher.pick(listOf(a, b, c), t))
    }

    @Test fun distanceAloneWhenNoRoadsYet() {
        val a = route(30.0, 50, "X Road")
        val b = route(22.1, 37, "Y Road")
        assertEquals(1, RouteMatcher.pick(listOf(a, b), GoogleTarget(22_000, null, emptyList())))
    }

    @Test fun noTargetKeepsTheMainRoute() =
        assertEquals(0, RouteMatcher.pick(listOf(route(30.0, 50, "A"), route(22.0, 36, "B")), null))
}
