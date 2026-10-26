package dev.rotos.app.share

import dev.rotos.app.net.AppIdentity
import dev.rotos.app.net.Http
import dev.rotos.app.quota.GoogleApi
import dev.rotos.app.quota.UsageGuard
import dev.rotos.app.route.LatLon
import org.json.JSONObject

/**
 * Google Places API (New) for shared places whose link carries only a name
 * (Google Maps search results often do). Two cheap calls: Text Search asking
 * for place ids only (free), then Place Details for the location (Essentials,
 * 10,000 free a month). Biased to [near] so a shop name finds the one in town.
 * Blocking: call on Dispatchers.IO.
 */
class PlacesClient(private val apiKey: String) {
    companion object {
        private const val SEARCH_URL = "https://places.googleapis.com/v1/places:searchText"
        private const val DETAILS_URL = "https://places.googleapis.com/v1/places/%s"
        private const val TIMEOUT_MS = 10_000
        private const val BIAS_M = 50_000.0

        fun searchBody(query: String, near: LatLon?): String {
            val body = JSONObject().put("textQuery", query).put("pageSize", 1)
            if (near != null) {
                val centre = JSONObject().put("latitude", near.lat).put("longitude", near.lon)
                body.put("locationBias", JSONObject().put("circle", JSONObject().put("center", centre).put("radius", BIAS_M)))
            }
            return body.toString()
        }

        /** Text Search answer -> first place id, or null. Pure. */
        fun firstId(json: String): String? =
            JSONObject(json).optJSONArray("places")?.optJSONObject(0)?.optString("id")?.ifEmpty { null }

        /** Place Details answer -> (position, address). Pure. */
        fun parseDetails(json: String): Pair<LatLon, String>? {
            val o = JSONObject(json)
            val loc = o.optJSONObject("location") ?: return null
            return LatLon(loc.getDouble("latitude"), loc.getDouble("longitude")) to o.optString("formattedAddress")
        }
    }

    fun find(query: String, near: LatLon?): Result<Pair<LatLon, String>> = runCatching {
        check(apiKey.isNotBlank()) { "no Google API key" }
        val day = UsageGuard.acquire(GoogleApi.PLACES, 1).getOrThrow()          // search and details: one each
        val search = Http.post(SEARCH_URL, searchBody(query, near).toByteArray(), "application/json", TIMEOUT_MS,
            headers("places.id")).onFailure { UsageGuard.refund(GoogleApi.PLACES, 1, day) }.getOrThrow()
        val id = firstId(String(search)) ?: throw IllegalArgumentException("Google found no place called \"$query\"")
        val day2 = UsageGuard.acquire(GoogleApi.PLACES, 1).getOrThrow()
        val details = Http.get(DETAILS_URL.format(id), TIMEOUT_MS, headers("location,formattedAddress"))
            .onFailure { UsageGuard.refund(GoogleApi.PLACES, 1, day2) }.getOrThrow()
        parseDetails(String(details)) ?: throw IllegalStateException("Google gave no location for \"$query\"")
    }

    private fun headers(fields: String) =
        mapOf("X-Goog-Api-Key" to apiKey, "X-Goog-FieldMask" to fields) + AppIdentity.googleHeaders()
}
