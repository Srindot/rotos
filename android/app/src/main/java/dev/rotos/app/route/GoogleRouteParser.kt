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
        // Google's main route first: if it cannot be used (too long), say why - never swap in an alternative quietly
        val main = routeOf(routes.getJSONObject(0), source).getOrThrow()
        listOf(main) + (1 until routes.length()).mapNotNull { routeOf(routes.getJSONObject(it), source).getOrNull() }
    }

    private fun routeOf(r: JSONObject, source: String): Result<Route> = runCatching {
        val steps = ArrayList<RouteStep>()
        val legs = r.optJSONArray("legs") ?: throw IllegalArgumentException("route has no legs")
        val total = (0 until legs.length()).sumOf { legs.getJSONObject(it).optJSONArray("steps")?.length() ?: 0 }
        // a cut route would end its line early while its distance / time describe all of it
        require(total <= MAX_STEPS) { "route too long for ROTOS: $total steps (at most $MAX_STEPS)" }
        for (li in 0 until legs.length()) {
            val ls = legs.getJSONObject(li).optJSONArray("steps") ?: continue
            for (si in 0 until ls.length()) {
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
