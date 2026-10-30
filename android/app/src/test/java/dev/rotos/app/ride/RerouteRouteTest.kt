package dev.rotos.app.ride

import dev.rotos.app.route.GoogleRoutesClient
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.Polyline
import dev.rotos.app.route.Route
import dev.rotos.app.route.RouteStep
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RerouteRouteTest {
    private fun route(vararg pts: LatLon, m: Int = 2_000) =
        Route(steps = listOf(RouteStep(Polyline.encode(pts.toList()), "", "Head east", m)), durationS = 300f, distanceM = m, source = "t")

    private val shown = route(LatLon(17.4000, 78.4000), LatLon(17.4000, 78.4100), LatLon(17.4100, 78.4100))

    @Test fun aParallelRoadToTheSameEndIsNotTheSameRoute() {
        // same length, same end, but 330 m north for most of the way: the rider needs its line
        val parallel = route(LatLon(17.4030, 78.4000), LatLon(17.4030, 78.4100), LatLon(17.4100, 78.4100))
        assertFalse(RerouteRules.sameRoute(shown, parallel))
    }

    @Test fun theRestOfTheShownRouteIsTheSameRoute() {
        val rest = route(LatLon(17.4000, 78.4050), LatLon(17.4000, 78.4100), LatLon(17.4100, 78.4100), m = 1_500)
        assertTrue(RerouteRules.sameRoute(shown, rest))
    }

    @Test fun reroutesTellGoogleWhichWayTheRiderIsGoing() {
        val body = JSONObject(GoogleRoutesClient.requestBody(LatLon(17.44, 78.35), LatLon(17.45, 78.5), 271.6f))
        assertEquals(272, body.getJSONObject("origin").getJSONObject("location").getInt("heading"))
        val still = JSONObject(GoogleRoutesClient.requestBody(LatLon(17.44, 78.35), LatLon(17.45, 78.5)))
        assertFalse(still.getJSONObject("origin").getJSONObject("location").has("heading"))
    }
}
