package dev.rotos.app.ride

import dev.rotos.app.route.LatLon
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ResumeRuleTest {
    private val home = LatLon(17.4455, 78.3489)            // IIIT Hyderabad
    private val dest = LatLon(17.3616, 78.4747)            // Charminar, ~16 km away
    private val now = 1_800_000_000_000L
    private val saved = SharedRoute(dest, now - 20 * 60_000L)

    @Test fun restartMidRideResumesTheSharedRoute() {
        assertEquals(dest, ResumeRule.resume(saved, now, home, 22_000))
        assertEquals(dest, ResumeRule.resume(saved, now, home, null))   // Google's summary not there yet
    }

    @Test fun staleOrFinishedRoutesAreNotResumed() {
        assertNull(ResumeRule.resume(null, now, home, 22_000))
        assertNull(ResumeRule.resume(saved.copy(savedMs = now - ResumeRule.MAX_AGE_MS - 1), now, home, 22_000))
        assertNull(ResumeRule.resume(saved, now, LatLon(17.3617, 78.4748), 100))      // already there
    }

    @Test fun anotherGoogleTripIsNotMistakenForIt() {
        assertNull(ResumeRule.resume(saved, now, home, 3_000))       // Google: 3 km left, dest 16 km away
        assertNull(ResumeRule.resume(saved, now, home, 90_000))      // Google: a far longer trip
    }
}
