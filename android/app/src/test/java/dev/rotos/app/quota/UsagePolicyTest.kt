package dev.rotos.app.quota

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class UsagePolicyTest {
    private val t0 = 1_000_000_000L

    @Test fun allowsUnderTheCaps() {
        assertEquals(Verdict.Allowed, UsagePolicy.decide(GoogleApi.ROUTES, Counts(5, 50, 0L), 1, Purpose.USER, t0))
    }

    @Test fun monthlyCapStopsCalls() {
        val c = Counts(0, GoogleApi.ROUTES.monthCap, 0L)
        assertTrue(UsagePolicy.decide(GoogleApi.ROUTES, c, 1, Purpose.USER, t0) is Verdict.Denied)
    }

    @Test fun dailyCapStopsCalls() {
        val c = Counts(GoogleApi.TILES.dayCap - 10, 100, 0L)
        assertEquals(Verdict.Allowed, UsagePolicy.decide(GoogleApi.TILES, c, 10, Purpose.USER, t0))
        assertTrue(UsagePolicy.decide(GoogleApi.TILES, c, 11, Purpose.USER, t0) is Verdict.Denied)
    }

    @Test fun automaticReroutesKeepTheirGap() {
        val c = Counts(1, 1, t0)
        val gap = GoogleApi.ROUTES.autoGapMs
        assertTrue(UsagePolicy.decide(GoogleApi.ROUTES, c, 1, Purpose.AUTO, t0 + gap - 1) is Verdict.Denied)
        assertEquals(Verdict.Allowed, UsagePolicy.decide(GoogleApi.ROUTES, c, 1, Purpose.AUTO, t0 + gap))
        // the rider sharing a place again is only kept from double taps
        assertEquals(Verdict.Allowed, UsagePolicy.decide(GoogleApi.ROUTES, c, 1, Purpose.USER, t0 + 5_000L))
    }

    @Test fun levelsWarnBeforeTheCap() {
        val api = GoogleApi.ROUTES
        assertEquals(Level.OK, UsagePolicy.level(api, Counts(0, 100, 0L)))
        assertEquals(Level.WARN, UsagePolicy.level(api, Counts(0, (api.monthCap * 0.8).toInt(), 0L)))
        assertEquals(Level.CAPPED, UsagePolicy.level(api, Counts(api.dayCap, 0, 0L)))
    }

    @Test fun newDayAndMonthStartFromZero() {
        val c = Counts(7, 70, t0)
        assertEquals(Counts(0, 70, t0), UsagePolicy.roll(c, sameDay = false, sameMonth = true))
        assertEquals(Counts(0, 0, t0), UsagePolicy.roll(c, sameDay = false, sameMonth = false))
        assertEquals(Counts(8, 71, t0 + 1), UsagePolicy.spend(c, 1, t0 + 1))
    }

    @Test fun capsStayUnderGooglesFreeTier() {
        assertTrue(GoogleApi.ROUTES.monthCap <= 5_000 / 2)
        assertTrue(GoogleApi.TILES.monthCap <= 100_000 / 2)
    }
}
