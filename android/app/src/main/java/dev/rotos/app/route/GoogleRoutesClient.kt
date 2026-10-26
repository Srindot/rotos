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

        fun requestBody(from: LatLon, to: LatLon): String = JSONObject()
            .put("origin", waypoint(from))
            .put("destination", waypoint(to))
            .put("travelMode", "TWO_WHEELER")
            .put("routingPreference", "TRAFFIC_AWARE")             // like the Google Maps app
            .put("computeAlternativeRoutes", true)
            .put("languageCode", "en")
            .put("units", "METRIC")
            .toString()

        private fun waypoint(p: LatLon) = JSONObject().put(
            "location", JSONObject().put(
                "latLng", JSONObject().put("latitude", p.lat).put("longitude", p.lon),
            ),
        )
    }

    /** Google's two-wheeler route and its alternatives (one billed call, if the caps allow it). */
    fun routes(from: LatLon, to: LatLon, purpose: Purpose = Purpose.USER): Result<List<Route>> {
        if (apiKey.isBlank()) return Result.failure(IllegalStateException("no Google API key"))
        val day = UsageGuard.acquire(GoogleApi.ROUTES, 1, purpose).getOrElse { return Result.failure(it) }
        val headers = mapOf("X-Goog-Api-Key" to apiKey, "X-Goog-FieldMask" to FIELD_MASK) + AppIdentity.googleHeaders()
        return Http.post(URL, requestBody(from, to).toByteArray(), "application/json", TIMEOUT_MS, headers)
            .onFailure { UsageGuard.refund(GoogleApi.ROUTES, 1, day) }          // never answered: not billed
            .mapCatching { GoogleRouteParser.parseAll(String(it), "google").getOrThrow() }
    }
}
