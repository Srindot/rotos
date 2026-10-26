package dev.rotos.app.ride

import dev.rotos.app.mirror.Guidance
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.ManeuverInfo
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class RerouteRulesTest {
    private val line = listOf(
        ManeuverInfo(1, 0, 500f, "Road No 36"),
        ManeuverInfo(2, 0, 2_000f, "Banjara Hills Main Road"),
    )
    private val along = Along(alongM = 400f, remainingM = 10_000f, remainingS = 900, wantsReroute = false)

    private fun google(road: String = "Road No 36", remainingM: Int? = 10_000) =
        Guidance(0, 200, remainingM, null, road, arrived = false)

    @Test fun engineOffRouteReroutesAtOnce() {
        val d = RerouteRules.decide(along.copy(wantsReroute = true), null, line, RerouteWatch(), 0L)
        assertTrue(d.reroute)
    }

    @Test fun agreeingGoogleKeepsTheLine() {
        val d = RerouteRules.decide(along, google(), line, RerouteWatch(), 1_000L)
        assertFalse(d.reroute)
        assertEquals(RerouteWatch(), d.watch)
    }

    @Test fun unknownRoadMustPersistBeforeRerouting() {
        val first = RerouteRules.decide(along, google("Necklace Road"), line, RerouteWatch(), 1_000L)
        assertFalse(first.reroute)
        assertNotNull(first.note)
        val later = RerouteRules.decide(along, google("Necklace Road"), line, first.watch, 1_000L + RerouteRules.MISMATCH_MS)
        assertTrue(later.reroute)
    }

    @Test fun distanceMismatchMustPersistBeforeRerouting() {
        val g = google(remainingM = 14_000)
        val first = RerouteRules.decide(along, g, line, RerouteWatch(), 1_000L)
        assertFalse(first.reroute)
        assertFalse(RerouteRules.decide(along, g, line, first.watch, 1_000L + RerouteRules.DISTANCE_MS - 1).reroute)
        assertTrue(RerouteRules.decide(along, g, line, first.watch, 1_000L + RerouteRules.DISTANCE_MS).reroute)
        // small differences never count
        assertEquals(0L, RerouteRules.decide(along, google(remainingM = 10_500), line, first.watch, 2_000L).watch.distanceSince)
    }

    @Test fun whileGoogleReroutesNothingHappensThenItsNewRouteIsTakenAtOnce() {
        val g = google("Somewhere Else").copy(rerouting = true)
        val during = RerouteRules.decide(along, g, line, RerouteWatch(1L, 1L), 100_000L)
        assertFalse(during.reroute)
        assertTrue(during.watch.googleRerouting)
        val after = RerouteRules.decide(along, google(), line, during.watch, 100_500L)     // even if it agrees with us
        assertTrue(after.reroute)
        assertEquals(RerouteWatch(), after.watch)
    }

    @Test fun onlyMovingWellLocatedFixesCount() {
        assertTrue(RerouteRules.trusted(8f, 12f))
        assertFalse(RerouteRules.trusted(0.3f, 12f))     // standing still
        assertFalse(RerouteRules.trusted(8f, 100f))      // indoors, GPS wandering
        assertFalse(RerouteRules.trusted(-1f, 5f))       // no speed known
    }

    @Test fun coversOnlyRoutesCloseToTheMapsRoute() {
        val path = listOf(LatLon(17.4000, 78.4000), LatLon(17.4000, 78.4100))
        assertTrue(RerouteRules.covers(listOf(LatLon(17.4005, 78.4020), LatLon(17.4000, 78.4090)), path))  // ~55 m
        assertFalse(RerouteRules.covers(listOf(LatLon(17.4000, 78.4020), LatLon(17.4030, 78.4050)), path)) // one ~330 m off
        assertFalse(RerouteRules.covers(path, emptyList()))
    }

    @Test fun aBypassStrayingInItsMiddleIsNotCovered() {
        val old = (0..20).map { LatLon(17.4000, 78.4000 + 0.0005 * it) }          // ~1 km east, dense
        // starts and ends on the old route, its middle ~330 m north: the segments are checked, not just the ends
        val bypass = listOf(LatLon(17.4000, 78.4000), LatLon(17.4030, 78.4050), LatLon(17.4000, 78.4100))
        assertFalse(RerouteRules.covers(bypass, old))
        val straight = listOf(LatLon(17.4000, 78.4000), LatLon(17.4000, 78.4100))   // the same road in 2 points
        assertTrue(RerouteRules.covers(straight, old))
    }

    @Test fun offMapOnlyBeyondTheEdge() {
        val path = listOf(LatLon(17.4000, 78.4000), LatLon(17.4000, 78.4100))
        assertFalse(RerouteRules.offMap(LatLon(17.4010, 78.4050), path))       // ~110 m away
        assertTrue(RerouteRules.offMap(LatLon(17.4100, 78.4050), path))        // ~1.1 km away
        assertFalse(RerouteRules.offMap(LatLon(0.0, 0.0), emptyList()))
    }
}
