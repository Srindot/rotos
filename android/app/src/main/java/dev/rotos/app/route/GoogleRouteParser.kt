package dev.rotos.app.route

import org.json.JSONObject

/** Google Routes API v2 computeRoutes JSON -> [Route]. Pure. */
object GoogleRouteParser {
    private const val MAX_STEPS = 400

    fun parse(json: String, source: String): Result<Route> = parseAll(json, source).map { it.first() }

    /** The main route and the alternatives (computeAlternativeRoutes). */
    fun parseAll(json: String, source: String): Result<List<Route>> = runCatching {
        val root = JSONObject(json)
        val routes = root.optJSONArray("routes")
        require(routes != null && routes.length() > 0) { "no routes in response" }
        (0 until routes.length()).mapNotNull { routeOf(routes.getJSONObject(it), source).getOrNull() }
            .also { require(it.isNotEmpty()) { "no usable route" } }
    }

    private fun routeOf(r: JSONObject, source: String): Result<Route> = runCatching {
        val steps = ArrayList<RouteStep>()
        val legs = r.optJSONArray("legs") ?: throw IllegalArgumentException("route has no legs")
        for (li in 0 until minOf(legs.length(), MAX_STEPS)) {
            val ls = legs.getJSONObject(li).optJSONArray("steps") ?: continue
            for (si in 0 until minOf(ls.length(), MAX_STEPS - steps.size)) {
                stepOf(ls.getJSONObject(si))?.let { steps.add(it) }
            }
        }
        require(steps.isNotEmpty()) { "route has no steps" }
        Route(
            steps = steps,
            durationS = parseDuration(r.optString("duration", "0s")),
            distanceM = r.optInt("distanceMeters", steps.sumOf { it.distanceM }),
            source = source,
        )
    }

    private fun stepOf(s: JSONObject): RouteStep? {
        val poly = s.optJSONObject("polyline")?.optString("encodedPolyline", "") ?: ""
        if (poly.isEmpty()) return null
        val ni = s.optJSONObject("navigationInstruction")
        return RouteStep(
            encodedPolyline = poly,
            maneuver = ni?.optString("maneuver", "") ?: "",
            instruction = ni?.optString("instructions", "") ?: "",
            distanceM = s.optInt("distanceMeters", 0),
        )
    }

    /** "642s" / "642.5s" -> 642.0 */
    fun parseDuration(s: String): Float = s.trim().removeSuffix("s").toFloatOrNull() ?: 0f
}
