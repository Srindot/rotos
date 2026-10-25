package dev.rotos.app.route

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class AreaRouteTest {
    private val c = LatLon(17.4455, 78.3489)

    @Test fun wholeSquareIsOnTheMap() {
        val pts = decode(AreaRoute.around(c).steps[0].encodedPolyline)
        for (i in -18..18) for (j in -18..18) {             // 100 m grid over the 3.6 km inner square
            val p = LatLon(c.lat + i * 0.1 / 110.574, c.lon + j * 0.1 / (111.320 * Math.cos(Math.toRadians(c.lat))))
            assertTrue("($i,$j)", AreaRoute.distanceToPath(p, pts) < 450.0)
        }
    }

    @Test fun outsideIsOffTheMap() {
        val pts = decode(AreaRoute.around(c).steps[0].encodedPolyline)
        val far = LatLon(c.lat + 3.0 / 110.574, c.lon)       // 3 km north of the centre
        assertTrue(AreaRoute.distanceToPath(far, pts) > 450.0)
    }

    @Test fun polylineRoundTrip() {
        val pts = listOf(LatLon(38.5, -120.2), LatLon(40.7, -120.95), LatLon(43.252, -126.453))
        assertEquals("_p~iF~ps|U_ulLnnqC_mqNvxq`@", Polyline.encode(pts))
    }

    /** Test-only decoder of the Google polyline format. */
    private fun decode(s: String): List<LatLon> {
        val out = ArrayList<LatLon>()
        var i = 0
        var lat = 0L
        var lon = 0L
        while (i < s.length) {
            val v = LongArray(2)
            for (k in 0..1) {
                var shift = 0
                var r = 0L
                while (true) {
                    val b = s[i++].code - 63
                    r = r or ((b and 0x1f).toLong() shl shift)
                    shift += 5
                    if (b < 0x20) break
                }
                v[k] = if (r and 1L != 0L) (r shr 1).inv() else r shr 1
            }
            lat += v[0]
            lon += v[1]
            out.add(LatLon(lat / 1e5, lon / 1e5))
        }
        return out
    }
}
