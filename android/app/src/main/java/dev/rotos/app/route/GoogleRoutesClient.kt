package dev.rotos.app.route

import dev.rotos.app.net.AppIdentity
import dev.rotos.app.net.Http
import dev.rotos.app.quota.GoogleApi
import dev.rotos.app.quota.Purpose
import dev.rotos.app.quota.UsageGuard
import org.json.JSONObject

/** Google Routes API v2 computeRoutes, travelMode TWO_WHEELER. Blocking: call on IO. */
class GoogleRoutesClient(private val apiKey: String) {
    companion object {
        const val URL = "https://routes.googleapis.com/directions/v2:computeRoutes"
        const val FIELD_MASK = "routes.duration,routes.distanceMeters,routes.polyline.encodedPolyline," +
            "routes.legs.steps.polyline.encodedPolyline,routes.legs.steps.navigationInstruction," +
            "routes.legs.steps.distanceMeters"
        private const val TIMEOUT_MS = 20_000

        /** [headingDeg]: the rider's course (0-359), so Google plans forward from the road they are on,
         *  not a U-turn back to the road they left; null when not moving. */
        /** [alternatives]: only when one is to be matched to what Google Maps shows (smaller otherwise). */
        fun requestBody(from: LatLon, to: LatLon, headingDeg: Float? = null, alternatives: Boolean = true): String = JSONObject()
            .put("origin", waypoint(from, headingDeg))
            .put("destination", waypoint(to))
            .put("travelMode", "TWO_WHEELER")
            .put("routingPreference", "TRAFFIC_AWARE")             // like the Google Maps app
            .put("computeAlternativeRoutes", alternatives)
            .put("languageCode", "en")
            .put("units", "METRIC")
            .toString()

        private fun waypoint(p: LatLon, headingDeg: Float? = null) = JSONObject().put(
            "location", JSONObject().put(
                "latLng", JSONObject().put("latitude", p.lat).put("longitude", p.lon),
            ).also { loc -> headingDeg?.takeIf { it.isFinite() }?.let { loc.put("heading", Math.floorMod(Math.round(it), 360)) } },
        )
    }

    /** Google's two-wheeler route and its alternatives (one billed call, if the caps allow it). */
    fun routes(
        from: LatLon, to: LatLon, purpose: Purpose = Purpose.USER, headingDeg: Float? = null, alternatives: Boolean = true,
    ): Result<List<Route>> {
        if (apiKey.isBlank()) return Result.failure(IllegalStateException("no Google API key"))
        val day = UsageGuard.acquire(GoogleApi.ROUTES, 1, purpose).getOrElse { return Result.failure(it) }
        val headers = mapOf("X-Goog-Api-Key" to apiKey, "X-Goog-FieldMask" to FIELD_MASK) + AppIdentity.googleHeaders()
        return Http.post(URL, requestBody(from, to, headingDeg, alternatives).toByteArray(), "application/json", TIMEOUT_MS, headers)
            .onFailure { UsageGuard.refund(GoogleApi.ROUTES, 1, day) }          // never answered: not billed
            .mapCatching { GoogleRouteParser.parseAll(String(it), "google").getOrThrow() }
    }
}
