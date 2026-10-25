package dev.rotos.app.share

import dev.rotos.app.route.LatLon
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PlacesClientTest {
    @Test fun searchIsBiasedNearTheRider() {
        val b = PlacesClient.searchBody("Shrey Automotives", LatLon(17.44, 78.48))
        assertTrue(b.contains("\"textQuery\":\"Shrey Automotives\"") && b.contains("\"circle\""))
    }

    @Test fun answersParse() {
        assertEquals("ChIJ123", PlacesClient.firstId("""{"places":[{"id":"ChIJ123"}]}"""))
        assertNull(PlacesClient.firstId("{}"))
        val d = PlacesClient.parseDetails("""{"location":{"latitude":17.44365,"longitude":78.48205},"formattedAddress":"Begumpet"}""")!!
        assertEquals(LatLon(17.44365, 78.48205), d.first)
        assertEquals("Begumpet", d.second)
    }
}
