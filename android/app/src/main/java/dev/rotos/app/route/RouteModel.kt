package dev.rotos.app.route

/** A WGS84 position. */
data class LatLon(val lat: Double, val lon: Double) {
    override fun toString(): String = "%.5f,%.5f".format(lat, lon)
}

/** One route step, as Google Routes gives it. */
data class RouteStep(
    val encodedPolyline: String,
    /** Google Routes API maneuver enum, e.g. "TURN_LEFT"; "" when absent. */
    val maneuver: String,
    val instruction: String,
    val distanceM: Int,
)

/** A route as the routing services return it, before it enters the C library. */
data class Route(
    val steps: List<RouteStep>,
    val durationS: Float,
    val distanceM: Int,
    /** Where it came from: "google", "area" or "sample". */
    val source: String,
)

/** The route once loaded into NativeNav. */
data class LoadedRoute(
    val routeId: Int,
    val totalM: Float,
    val durationS: Float,
    val points: List<LatLon>,
    val maneuvers: List<ManeuverInfo>,
    val source: String,
)

data class ManeuverInfo(val type: Int, val hint: Int, val alongM: Float, val road: String)
