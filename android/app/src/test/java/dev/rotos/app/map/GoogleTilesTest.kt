package dev.rotos.app.map

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class GoogleTilesTest {
    /** Every colour the style paints must be one the C classifier knows exactly. */
    @Test fun styleUsesOnlyClassifierColours() {
        val known = setOf("#f2efe9", "#e0dfdf", "#c8facc", "#aad3df", "#d9d0c9", "#ffffff", "#f7fabf", "#fcd6a4", "#e892a2")
        val body = GoogleTiles.sessionBody()
        val colours = Regex("\"color\":\"(#[0-9a-f]{6})\"").findAll(body).map { it.groupValues[1] }.toSet()
        assertTrue(colours.isNotEmpty())
        assertTrue("unknown colours: ${colours - known}", known.containsAll(colours))
        assertTrue(body.contains("\"visibility\":\"off\""))
        assertTrue(body.contains("\"mapType\":\"roadmap\""))
    }

    @Test fun parsesSession() {
        val (token, expiry) = GoogleTiles.parseSession("""{"session":"abc","expiry":"1361398412","tileWidth":256}""")
        assertEquals("abc", token)
        assertEquals(1361398412L, expiry)
    }
}
