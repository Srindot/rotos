package dev.rotos.app.route

import android.content.res.AssetManager
import dev.rotos.app.quota.Purpose
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Where routes come from: Google Routes API (two-wheeler, with alternatives),
 * or the bundled sample (assets/sample_route.json, a Google Routes v2 response).
 */
class RouteRepository(private val apiKey: String, private val assets: AssetManager) {
    companion object {
        const val SAMPLE_ASSET = "sample_route.json"
    }

    val usesGoogle: Boolean get() = apiKey.isNotBlank()

    /** Google's candidate routes; fails without a key, a network or headroom under the caps. */
    suspend fun alternatives(
        from: LatLon, to: LatLon, purpose: Purpose = Purpose.USER, headingDeg: Float? = null, alternatives: Boolean = true,
    ): Result<List<Route>> = withContext(Dispatchers.IO) { GoogleRoutesClient(apiKey).routes(from, to, purpose, headingDeg, alternatives) }

    suspend fun sample(): Result<Route> = withContext(Dispatchers.IO) {
        runCatching { assets.open(SAMPLE_ASSET).use { String(it.readBytes()) } }
            .mapCatching { GoogleRouteParser.parse(it, "sample").getOrThrow() }
    }
}
