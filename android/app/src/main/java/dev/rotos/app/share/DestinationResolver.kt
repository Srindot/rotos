package dev.rotos.app.share

import android.content.Context
import android.location.Address
import android.location.Geocoder
import dev.rotos.app.BuildConfig
import dev.rotos.app.location.CurrentLocation
import dev.rotos.app.route.LatLon
import dev.rotos.app.util.AppLog
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.util.Locale

/** Where the ride goes, what to call it on screen, and its address line. */
data class Destination(val label: String, val at: LatLon, val detail: String = "")

/**
 * Destination text -> position: "lat,lon", a Google Maps share / link / geo:
 * URI (MapsLink), or a place name (android.location.Geocoder).
 */
class DestinationResolver(private val context: Context) {
    companion object {
        val IIIT_HYDERABAD = LatLon(17.4455, 78.3489)
        val DURGAM_CHERUVU = LatLon(17.4300, 78.3890)

        /** Pure: parses "17.43, 78.389"; null when it is not a coordinate pair. */
        fun parseLatLon(text: String): LatLon? {
            val parts = text.split(',').map { it.trim() }
            if (parts.size != 2) return null
            val lat = parts[0].toDoubleOrNull() ?: return null
            val lon = parts[1].toDoubleOrNull() ?: return null
            if (lat !in -85.0..85.0 || lon !in -180.0..180.0) return null
            return LatLon(lat, lon)
        }

        /** Great-circle distance in metres. Pure. */
        fun distanceM(a: LatLon, b: LatLon): Double {
            val r = 6_371_000.0
            val p1 = Math.toRadians(a.lat)
            val p2 = Math.toRadians(b.lat)
            val dp = p2 - p1
            val dl = Math.toRadians(b.lon - a.lon)
            val h = Math.sin(dp / 2) * Math.sin(dp / 2) + Math.cos(p1) * Math.cos(p2) * Math.sin(dl / 2) * Math.sin(dl / 2)
            return 2 * r * Math.asin(Math.min(1.0, Math.sqrt(h)))
        }

        /** "IIIT Hyderabad" + "Gachibowli, Hyderabad, Telangana 500032" from a geocoder result. */
        fun fromAddress(query: String, a: Address): Destination {
            val line = a.getAddressLine(0).orEmpty()
            val name = a.featureName?.takeIf { it.isNotBlank() && it.toDoubleOrNull() == null && it.length > 2 }
                ?: line.substringBefore(',').ifBlank { query }
            val detail = line.removePrefix(name).trimStart(',', ' ').ifBlank { listOfNotNull(a.locality, a.adminArea).joinToString(", ") }
            return Destination(name, LatLon(a.latitude, a.longitude), detail)
        }
    }

    /**
     * Up to [n] places for [query], nearest first: searched within ~110 km of
     * [near] first (so "iiit" finds the one in town, not one 700 km away),
     * then anywhere if nothing is close.
     */
    suspend fun search(query: String, near: LatLon?, n: Int = 5): Result<List<Destination>> {
        val q = query.trim()
        if (q.length < 2) return Result.success(emptyList())
        if (!Geocoder.isPresent()) return Result.failure(IllegalStateException("No geocoder on this phone: enter lat,lon"))
        return withContext(Dispatchers.IO) {
            runCatching {
                val g = Geocoder(context, Locale.getDefault())
                @Suppress("DEPRECATION")
                val local = near?.let { c -> g.getFromLocationName(q, n, c.lat - 1.0, c.lon - 1.0, c.lat + 1.0, c.lon + 1.0) }.orEmpty()
                @Suppress("DEPRECATION")
                val found = local.ifEmpty { g.getFromLocationName(q, n).orEmpty() }
                found.map { fromAddress(q, it) }
                    .sortedBy { d -> near?.let { distanceM(it, d.at) } ?: 0.0 }
            }
        }
    }

    suspend fun resolve(text: String): Result<LatLon> = resolvePlace(text).map { it.at }

    suspend fun resolvePlace(text: String): Result<Destination> {
        val t = text.trim()
        if (t.isEmpty()) return Result.failure(IllegalArgumentException("Enter a destination first"))
        parseLatLon(t)?.let { return Result.success(Destination(it.toString(), it)) }
        if (MapsLink.looksLikeLink(t)) return fromLink(t)
        return search(t, near = null, n = 1).mapCatching { it.firstOrNull() ?: throw IllegalArgumentException("Place not found: $t") }
    }

    private suspend fun fromLink(text: String): Result<Destination> = withContext(Dispatchers.IO) {
        val label = MapsLink.label(text)
        val raw = if (text.startsWith("geo:")) text else MapsLink.firstUrl(text) ?: text
        MapsLink.coords(raw)?.let { return@withContext Result.success(Destination(label ?: it.toString(), it)) }
        val full = if (raw.startsWith("geo:")) raw else MapsLink.expand(raw).getOrElse {
            AppLog.i("share: could not expand $raw: ${it.javaClass.simpleName}: ${it.message}")
            return@withContext Result.failure(IllegalStateException("Could not open the Google Maps link. Check the internet and share it again."))
        }
        MapsLink.coords(full)?.let { return@withContext Result.success(Destination(label ?: it.toString(), it)) }
        val q = MapsLink.query(full) ?: label
            ?: return@withContext Result.failure(IllegalArgumentException("No place in that link"))
        byName(q).map { (at, address) -> Destination(label ?: q, at, address) }
    }

    /**
     * A place known only by name (Google Maps search results share just that):
     * Google Places near the rider first, the phone's geocoder as a fallback.
     */
    private fun byName(q: String): Result<Pair<LatLon, String>> {
        val near = CurrentLocation.lastKnown(context)
        val google = PlacesClient(BuildConfig.MAPS_API_KEY).find(q, near)
        if (google.isSuccess) return google
        AppLog.i("share: Places lookup failed (${google.exceptionOrNull()?.message?.take(120)}), trying the geocoder")
        return geocodeBlocking(q).map { it to "" }
    }

    /** The phone's geocoder; blocking (call on Dispatchers.IO). */
    private fun geocodeBlocking(q: String): Result<LatLon> {
        if (!Geocoder.isPresent()) return Result.failure(IllegalStateException("No geocoder on this phone: enter lat,lon"))
        return runCatching {
            @Suppress("DEPRECATION")
            val a = Geocoder(context, Locale.getDefault()).getFromLocationName(q, 1)
            val first = a?.firstOrNull() ?: throw IllegalArgumentException("Place not found: $q")
            LatLon(first.latitude, first.longitude)
        }
    }
}
