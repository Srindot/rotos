package dev.rotos.app.route

/**
 * A route-shaped path that sweeps a square around [centre], so the normal map
 * pack recipe (corridor = route + ~620 m each side) maps the whole area. Used
 * when Google Maps navigates and ROTOS only needs the map around the rider.
 * Rows run east-west, [ROW_KM] apart, alternating direction. Pure.
 */
object AreaRoute {
    /** Metres from [p] to the polyline [pts] (segments, local flat projection). Pure. */
    fun distanceToPath(p: LatLon, pts: List<LatLon>): Double {
        if (pts.isEmpty()) return Double.MAX_VALUE
        val kx = 111_320.0 * Math.cos(Math.toRadians(p.lat))
        val ky = 110_574.0
        var best = Double.MAX_VALUE
        for (i in pts.indices) {
            val ax = (pts[i].lon - p.lon) * kx
            val ay = (pts[i].lat - p.lat) * ky
            if (i == pts.size - 1) {
                best = minOf(best, Math.hypot(ax, ay))
                break
            }
            val bx = (pts[i + 1].lon - p.lon) * kx
            val by = (pts[i + 1].lat - p.lat) * ky
            val dx = bx - ax
            val dy = by - ay
            val len2 = dx * dx + dy * dy
            val t = if (len2 > 0) ((-ax * dx - ay * dy) / len2).coerceIn(0.0, 1.0) else 0.0
            best = minOf(best, Math.hypot(ax + t * dx, ay + t * dy))
        }
        return best
    }

    const val HALF_KM = 2.0
    /** Rows closer than twice the off-map threshold (450 m), so the whole square counts as "on the map". */
    private const val ROW_KM = 0.8
    private const val STEP_KM = 0.25

    fun around(centre: LatLon, halfKm: Double = HALF_KM): Route {
        val dLat = 1.0 / 110.574
        val dLon = 1.0 / (111.320 * Math.cos(Math.toRadians(centre.lat)))
        val pts = ArrayList<LatLon>()
        var north = -halfKm
        var east = true
        while (north <= halfKm + 1e-9) {
            var x = if (east) -halfKm else halfKm
            val end = if (east) halfKm else -halfKm
            val step = if (east) STEP_KM else -STEP_KM
            while (if (east) x <= end + 1e-9 else x >= end - 1e-9) {
                pts.add(LatLon(centre.lat + north * dLat, centre.lon + x * dLon))
                x += step
            }
            north += ROW_KM
            east = !east
        }
        val metres = pts.zipWithNext { a, b -> distance(a, b) }.sum().toInt()
        return Route(
            steps = listOf(RouteStep(encodedPolyline = Polyline.encode(pts), maneuver = "DEPART",
                instruction = "", distanceM = metres)),
            durationS = 0f, distanceM = metres, source = "area",
        )
    }

    private fun distance(a: LatLon, b: LatLon): Double {
        val dy = (b.lat - a.lat) * 110_574.0
        val dx = (b.lon - a.lon) * 111_320.0 * Math.cos(Math.toRadians(a.lat))
        return Math.sqrt(dx * dx + dy * dy)
    }
}

/** Google encoded polyline format (precision 1e-5). Pure. */
object Polyline {
    fun encode(points: List<LatLon>): String {
        val sb = StringBuilder()
        var pLat = 0L
        var pLon = 0L
        for (p in points) {
            val lat = Math.round(p.lat * 1e5)
            val lon = Math.round(p.lon * 1e5)
            put(sb, lat - pLat)
            put(sb, lon - pLon)
            pLat = lat
            pLon = lon
        }
        return sb.toString()
    }

    /** Google encoded polyline -> points (bounded by the string length). Pure. */
    fun decode(s: String): List<LatLon> {
        val out = ArrayList<LatLon>()
        var i = 0
        var lat = 0L
        var lon = 0L
        while (i < s.length) {
            val (dLat, i1) = take(s, i)
            val (dLon, i2) = take(s, i1)
            if (i2 <= i1) break
            lat += dLat
            lon += dLon
            out.add(LatLon(lat / 1e5, lon / 1e5))
            i = i2
        }
        return out
    }

    /** One zig-zag varint from [from]: (value, next index). */
    private fun take(s: String, from: Int): Pair<Long, Int> {
        var result = 0L
        var shift = 0
        var i = from
        while (i < s.length && shift < 64) {
            val b = s[i++].code - 63
            result = result or ((b and 0x1f).toLong() shl shift)
            shift += 5
            if (b < 0x20) return (if (result and 1L != 0L) (result shr 1).inv() else result shr 1) to i
        }
        return 0L to from
    }

    private fun put(sb: StringBuilder, v: Long) {
        var x = if (v < 0) (v shl 1).inv() else v shl 1
        while (x >= 0x20) {
            sb.append(((0x20 or (x and 0x1f).toInt()) + 63).toChar())
            x = x shr 5
        }
        sb.append((x + 63).toInt().toChar())
    }
}
