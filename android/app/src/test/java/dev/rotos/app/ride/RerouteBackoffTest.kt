package dev.rotos.app.ride

import dev.rotos.app.route.LatLon
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RerouteBackoffTest {
    private fun ride() = ActiveRide(RideConfig(origin = LatLon(17.44, 78.35), destination = LatLon(17.45, 78.5),
        useSample = false, demo = false, speedup = 1f, link = LinkKind.BLE, host = "", port = 0))

    @Test fun aRerouteThatSettlesThingsHoldsNothing() {
        val r = ride()
        r.rerouted(same = false, now = 1_000_000L)
        assertEquals(1_000_000L, r.holdUntilMs)
    }

    @Test fun repeatsBackOffAndDouble() {
        val r = ride()
        var t = 1_000_000L
        r.rerouted(false, t)                                  // first: free
        t += 10_000L; r.rerouted(false, t)                    // a repeat 10 s later: 30 s hold
        assertEquals(t + 30_000L, r.holdUntilMs)
        t += 40_000L; r.rerouted(false, t)                    // again: 60 s
        assertEquals(t + 60_000L, r.holdUntilMs)
        t += 70_000L; r.rerouted(true, t)                     // and a no-change: 120 s
        assertEquals(t + 120_000L, r.holdUntilMs)
        repeat(10) { t += 700_000L; r.rerouted(true, t) }     // never more than 10 min
        assertTrue(r.holdUntilMs - t <= ActiveRide.HOLD_MAX_MS)
    }

    @Test fun aQuietSpellResetsTheStreak() {
        val r = ride()
        r.rerouted(false, 0L + 1)
        r.rerouted(false, 10_000L)                            // repeat: held until 40 s
        val later = 40_000L + ActiveRide.REPEAT_MS + 1        // quiet for REPEAT_MS after the hold ended
        r.rerouted(false, later)
        assertEquals(later, r.holdUntilMs)
    }

    @Test fun neverMoreThanTwentyAnHour() {
        val r = ride()
        var t = 0L
        repeat(ActiveRide.MAX_PER_HOUR) { t += 3 * 60_000L; r.rerouted(false, t) }   // spaced out: no streak
        assertTrue(r.capUntilMs > t + 60_000L)                // the hour's budget is spent: held until it frees up
        assertFalse(r.mayReroute(urgent = true, now = t + 60_000L))   // not even a wrong turn
    }

    @Test fun aDisagreementThatComesBackAfterEveryHoldKeepsGrowingIt() {
        val r = ride()
        var t = 1_000L
        r.rerouted(false, t)
        var last = 0L
        repeat(8) {                                           // fires again 6 s after each hold ends
            t = maxOf(r.holdUntilMs, t) + 6_000L
            r.rerouted(false, t)
            assertTrue(r.holdUntilMs - t >= last)             // never resets
            last = r.holdUntilMs - t
        }
        assertEquals(ActiveRide.HOLD_MAX_MS, last)
    }

    @Test fun aWrongTurnWaitsAtMostThirtySecondsOfAHold() {
        val r = ride()
        var t = 0L
        repeat(5) { t += 10_000L; r.rerouted(false, t) }      // a long hold now
        assertFalse(r.mayReroute(urgent = false, now = t + 31_000L))
        assertTrue(r.mayReroute(urgent = true, now = t + 31_000L))
    }

    @Test fun pastTheHoursCapAWrongTurnStillGetsALineEveryTwoMinutes() {
        val r = ride()
        var t = 0L
        repeat(ActiveRide.MAX_PER_HOUR) { t += 3 * 60_000L; r.rerouted(false, t) }
        assertFalse(r.mayReroute(urgent = false, now = t + 130_000L))     // a mere disagreement waits out the hour
        assertFalse(r.mayReroute(urgent = true, now = t + 60_000L))
        assertTrue(r.mayReroute(urgent = true, now = t + 121_000L))      // the line never freezes for long
    }
}
