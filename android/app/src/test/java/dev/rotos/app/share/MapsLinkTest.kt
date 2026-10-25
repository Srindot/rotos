package dev.rotos.app.share

import dev.rotos.app.route.LatLon
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MapsLinkTest {
    private fun near(a: LatLon?, lat: Double, lon: Double) {
        requireNotNull(a)
        assertEquals(lat, a.lat, 1e-6)
        assertEquals(lon, a.lon, 1e-6)
    }

    @Test fun exactPlaceBeatsMapCentre() = near(
        MapsLink.coords("https://www.google.com/maps/place/Charminar/@17.3615,78.4746,17z/data=!3m1!4b1!4m6!3m5!1s0x0:0x0!8m2!3d17.3616078!4d78.4746713"),
        17.3616078, 78.4746713)

    @Test fun queryParameter() = near(MapsLink.coords("https://maps.google.com/?q=17.4300,78.3890"), 17.43, 78.389)

    @Test fun encodedComma() = near(MapsLink.coords("https://www.google.com/maps/search/?api=1&query=17.43%2C78.389"), 17.43, 78.389)

    @Test fun mapCentre() = near(MapsLink.coords("https://www.google.com/maps/@17.4455,78.3489,15z"), 17.4455, 78.3489)

    @Test fun geoUri() = near(MapsLink.coords("geo:17.36,78.47"), 17.36, 78.47)

    @Test fun geoZeroIsNotAPlace() {
        assertNull(MapsLink.coords("geo:0,0?q=Charminar"))
        assertEquals("Charminar", MapsLink.query("geo:0,0?q=Charminar"))
    }

    @Test fun geoQueryWithCoords() = near(MapsLink.coords("geo:0,0?q=17.36,78.47(Home)"), 17.36, 78.47)

    @Test fun placeNameFromUrl() =
        assertEquals("Golconda Fort", MapsLink.query("https://www.google.com/maps/place/Golconda+Fort/data=!4m2"))

    @Test fun shareTextLabelAndUrl() {
        val share = "Charminar\nCharminar Rd, Hyderabad, Telangana 500002\nhttps://maps.app.goo.gl/AbCdEf123"
        assertTrue(MapsLink.looksLikeLink(share))
        assertEquals("Charminar", MapsLink.label(share))
        assertEquals("https://maps.app.goo.gl/AbCdEf123", MapsLink.firstUrl(share))
        assertNull(MapsLink.coords(MapsLink.firstUrl(share)!!))
    }

    @Test fun plainTextIsNotALink() = assertTrue(!MapsLink.looksLikeLink("Durgam Cheruvu"))
}
