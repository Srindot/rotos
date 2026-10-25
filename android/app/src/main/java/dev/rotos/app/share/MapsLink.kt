package dev.rotos.app.share

import dev.rotos.app.net.Http
import dev.rotos.app.route.LatLon
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLDecoder

/**
 * Google Maps links -> a position, without an API key.
 *
 * Shares from the Google Maps app look like
 *   "Charminar\nCharminar Rd, Hyderabad\nhttps://maps.app.goo.gl/AbC123"
 * The short link redirects to a full google.com/maps URL, which carries the
 * place as "!3dLAT!4dLON" (exact), "?q=LAT,LON", or "@LAT,LON" (map centre).
 * geo: URIs ("geo:17.36,78.47" or "geo:0,0?q=17.36,78.47(Label)" or
 * "geo:0,0?q=Some+address") are handled here too.
 */
object MapsLink {
    private const val TIMEOUT_MS = 4_000                  // resets are instant; do not wait long per try
    private const val MAX_REDIRECTS = 6
    private val URL_RE = Regex("""https?://\S+""")
    private val COORD_PATTERNS = listOf(
        Regex("""!3d(-?\d{1,2}\.\d+)!4d(-?\d{1,3}\.\d+)"""),
        Regex("""[?&](?:q|query|destination|daddr|ll|center)=(-?\d{1,2}\.\d+),\s*(-?\d{1,3}\.\d+)"""),
        Regex("""/dir/[^/]*/(-?\d{1,2}\.\d+),\s*(-?\d{1,3}\.\d+)"""),
        Regex("""@(-?\d{1,2}\.\d+),(-?\d{1,3}\.\d+)"""),
        Regex("""^geo:(-?\d{1,2}\.\d+),(-?\d{1,3}\.\d+)"""),
    )
    private val PLACE_RE = Regex("""/maps/place/([^/@?]+)""")
    private const val EXPAND_TRIES = 8
    private const val EXPAND_PAUSE_MS = 150L

    /** A shared text or URL worth treating as a Google Maps / geo link. */
    fun looksLikeLink(text: String): Boolean =
        text.trimStart().startsWith("geo:") || URL_RE.containsMatchIn(text)

    /** First line of a share that is not the link itself: the place name. */
    fun label(text: String): String? = text.lines().map { it.trim() }
        .firstOrNull { it.isNotEmpty() && !URL_RE.containsMatchIn(it) && !it.startsWith("geo:") }

    /** Pure: coordinates written in a (decoded) URL or geo: URI, else null. */
    fun coords(url: String): LatLon? {
        val u = decode(url)
        for (re in COORD_PATTERNS) {
            val m = re.find(u) ?: continue
            val lat = m.groupValues[1].toDoubleOrNull() ?: continue
            val lon = m.groupValues[2].toDoubleOrNull() ?: continue
            if (lat in -85.0..85.0 && lon in -180.0..180.0 && !(lat == 0.0 && lon == 0.0)) return LatLon(lat, lon)
        }
        return null
    }

    /** Pure: a place name or address carried by the URL ("/maps/place/NAME", geo:0,0?q=ADDRESS). */
    fun query(url: String): String? {
        val u = decode(url)
        PLACE_RE.find(u)?.let { return it.groupValues[1].replace('+', ' ').trim().ifEmpty { null } }
        if (u.startsWith("geo:")) {
            val q = u.substringAfter("?q=", "").substringBefore('&').replace('+', ' ').trim()
            return q.substringBefore('(').trim().ifEmpty { null }
        }
        return null
    }

    /**
     * Blocking (call on Dispatchers.IO): follows short-link redirects, returns
     * the final URL. Some mobile networks reset the TLS handshake with
     * maps.app.goo.gl now and then ("connection reset", "decode error"; often
     * 2 tries in 3), so it retries quickly, up to [EXPAND_TRIES] times
     * (~6 s), alternating with a trailing "/" (the same link).
     */
    fun expand(url: String): Result<String> {
        var last: Throwable = IllegalStateException("no attempt")
        for (i in 0 until EXPAND_TRIES) {
            val u = if (i % 2 == 0) url else url.trimEnd('/') + "/"
            expandOnce(u).onSuccess { return Result.success(it) }.onFailure { last = it }
            if (i < EXPAND_TRIES - 1) Thread.sleep(EXPAND_PAUSE_MS * (i + 1))
        }
        return Result.failure(last)
    }

    private fun expandOnce(url: String): Result<String> = runCatching {
        var cur = url
        for (i in 0 until MAX_REDIRECTS) {
            if (coords(cur) != null) return@runCatching cur
            val c = URL(cur).openConnection() as HttpURLConnection
            val next = try {
                c.instanceFollowRedirects = false
                c.connectTimeout = TIMEOUT_MS
                c.readTimeout = TIMEOUT_MS
                c.setRequestProperty("User-Agent", Http.USER_AGENT)
                if (c.responseCode !in 300..399) return@runCatching cur
                c.getHeaderField("Location") ?: return@runCatching cur
            } finally {
                c.disconnect()
            }
            // consent / interstitial pages carry the real target in "continue="
            cur = URL(URL(cur), next).toString().let { n ->
                if (n.contains("continue=")) decode(n.substringAfter("continue=").substringBefore('&')) else n
            }
        }
        cur
    }

    fun firstUrl(text: String): String? = URL_RE.find(text)?.value?.trimEnd('.', ',', ')')

    private fun decode(s: String): String = runCatching { URLDecoder.decode(s, "UTF-8") }.getOrDefault(s)
}
