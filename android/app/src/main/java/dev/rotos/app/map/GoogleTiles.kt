package dev.rotos.app.map

import dev.rotos.app.net.AppIdentity
import dev.rotos.app.net.Http
import dev.rotos.app.quota.GoogleApi
import dev.rotos.app.quota.Purpose
import dev.rotos.app.quota.UsageGuard
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Google Map Tiles API (2D roadmap) as a ROTOS map source.
 *
 * The session asks Google to draw the map in a custom style that uses exactly
 * the reference colours of core/src/map/rn_mapgen_classify.c (land, built-up,
 * parks, water, buildings, minor / major roads) with every label and icon
 * switched off, so the existing classifier reads Google's map without any
 * guessing. Session tokens live ~2 weeks; one is kept in cacheDir and reused.
 * Blocking: call on Dispatchers.IO.
 */
class GoogleTiles(private val apiKey: String, private val cacheDir: File) {
    companion object {
        const val SESSION_URL = "https://tile.googleapis.com/v1/createSession?key=%s"
        const val TILE_URL = "https://tile.googleapis.com/v1/2dtiles/%d/%d/%d?session=%s&key=%s"
        private const val TIMEOUT_MS = 15_000
        /** Bump when [STYLE] changes: cached tiles are kept per style version. */
        const val STYLE_VERSION = 4
        /** Local road width (px at 256 px tiles); found by running Hyderabad tiles through rn_mapgen. */
        private const val LOCAL_WEIGHT = 8

        // rn_mapgen_classify.c reference colours
        private const val LAND = "#f2efe9"
        private const val LANDUSE = "#e0dfdf"
        private const val PARK = "#c8facc"
        private const val WATER = "#aad3df"
        private const val ROAD_MINOR = "#ffffff"
        private const val ROAD_SECONDARY = "#f7fabf"
        private const val ROAD_PRIMARY = "#fcd6a4"
        private const val ROAD_MOTORWAY = "#e892a2"

        /** (featureType, elementType, stylers) rules, applied in order. */
        private val STYLE: List<Triple<String, String, List<Pair<String, Any>>>> = listOf(
            Triple("all", "labels", listOf("visibility" to "off")),
            Triple("all", "geometry.stroke", listOf("visibility" to "off")),
            Triple("administrative", "all", listOf("visibility" to "off")),
            Triple("transit", "all", listOf("visibility" to "off")),
            Triple("poi", "all", listOf("visibility" to "off")),
            Triple("landscape", "geometry.fill", listOf("color" to LAND)),
            // Google draws buildings / built-up blocks ("man made") ON TOP of roads: in
            // Hyderabad they hid 6-28 % of road pixels (streets cut into pieces). Off.
            Triple("landscape.man_made", "all", listOf("visibility" to "off")),
            Triple("landscape.natural.landcover", "geometry.fill", listOf("color" to LAND)),
            Triple("poi.park", "geometry.fill", listOf("visibility" to "on", "color" to PARK)),
            Triple("poi.sports_complex", "geometry.fill", listOf("visibility" to "on", "color" to PARK)),
            Triple("poi.school", "geometry.fill", listOf("visibility" to "on", "color" to LANDUSE)),
            Triple("poi.medical", "geometry.fill", listOf("visibility" to "on", "color" to LANDUSE)),
            Triple("water", "geometry.fill", listOf("color" to WATER)),
            // weight 8: Google's thin local roads would fall apart in the classifier (gaps, dead ends)
            Triple("road.local", "geometry.fill", listOf("color" to ROAD_MINOR, "weight" to LOCAL_WEIGHT)),
            Triple("road.arterial", "geometry.fill", listOf("color" to ROAD_SECONDARY)),
            Triple("road.highway", "geometry.fill", listOf("color" to ROAD_PRIMARY)),
            Triple("road.highway.controlled_access", "geometry.fill", listOf("color" to ROAD_MOTORWAY)),
        )

        /** createSession body: roadmap, 256 px tiles, English, India, the ROTOS style. Pure. */
        fun sessionBody(): String {
            val styles = JSONArray()
            for ((feature, element, stylers) in STYLE) {
                val arr = JSONArray()
                stylers.forEach { (k, v) -> arr.put(JSONObject().put(k, v)) }
                styles.put(JSONObject().put("featureType", feature).put("elementType", element).put("stylers", arr))
            }
            return JSONObject()
                .put("mapType", "roadmap")
                .put("language", "en-US")
                .put("region", "IN")
                .put("scale", "scaleFactor1x")
                .put("highDpi", false)
                .put("styles", styles)
                .toString()
        }

        /** createSession answer -> (token, expiry unix seconds). Pure. */
        fun parseSession(json: String): Pair<String, Long> {
            val o = JSONObject(json)
            return o.getString("session") to o.optString("expiry", "0").toLong()
        }
    }

    private val sessionFile = File(cacheDir, "gmap/session-v$STYLE_VERSION.json")

    /** A valid session token (reused until a day before it expires; a new one counts against the caps). */
    fun session(purpose: Purpose = Purpose.USER): Result<String> = runCatching {
        cached()?.let { return@runCatching it }
        UsageGuard.acquire(GoogleApi.TILE_SESSION, 1, purpose).getOrThrow()
        val bytes = Http.post(SESSION_URL.format(apiKey), sessionBody().toByteArray(), "application/json", TIMEOUT_MS,
            AppIdentity.googleHeaders())
            .getOrThrow()
        val (token, expiry) = parseSession(String(bytes))
        sessionFile.parentFile?.mkdirs()
        sessionFile.writeText(JSONObject().put("session", token).put("expiry", expiry).toString())
        token
    }

    private fun cached(): String? = runCatching {
        val o = JSONObject(sessionFile.readText())
        if (o.getLong("expiry") - System.currentTimeMillis() / 1000 > 86_400) o.getString("session") else null
    }.getOrNull()

    fun tileUrl(z: Int, x: Int, y: Int, session: String): String = TILE_URL.format(z, x, y, session, apiKey)
}
