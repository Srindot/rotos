package dev.rotos.app.mirror

import dev.rotos.app.mirror.GuidanceParser as P
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.time.LocalTime

/** Notification texts in the shapes Google Maps uses (English). */
class GuidanceParserTest {
    private val now = LocalTime.of(17, 40)

    @Test fun distanceTitleAndInstruction() {
        val g = P.parse("250 m", "Turn right onto Old Mumbai Hwy", "12 min · 4.2 km · 17:58 ETA", now)!!
        assertEquals(P.M_RIGHT, g.maneuver)
        assertEquals(250, g.toTurnM)
        assertEquals(4200, g.remainingM)
        assertEquals(LocalTime.of(17, 58), g.eta)
        assertEquals("Old Mumbai Hwy", g.road)
    }

    @Test fun kilometresAndTwelveHourEta() {
        val g = P.parse("1.2 km", "Slight left toward Gachibowli", "25 min · 9.8 km · 6:05 PM", now)!!
        assertEquals(P.M_SLIGHT_LEFT, g.maneuver)
        assertEquals(1200, g.toTurnM)
        assertEquals(9800, g.remainingM)
        assertEquals(LocalTime.of(18, 5), g.eta)
        assertEquals("Gachibowli", g.road)
    }

    @Test fun etaFromMinutesWhenNoClockTime() {
        val g = P.parse("300 m", "Turn left", "1 hr 5 min (32 km)", now)!!
        assertEquals(P.M_LEFT, g.maneuver)
        assertEquals(LocalTime.of(18, 45), g.eta)
        assertEquals(32000, g.remainingM)
    }

    @Test fun roundaboutExit() {
        val g = P.parse("Toward Road No. 36", "In 80 m, at the roundabout, take the 2nd exit onto Road No. 36", "", now)!!
        assertEquals(P.M_ROUNDABOUT_LEFT, g.maneuver)
        assertEquals("Road No. 36", g.road)
    }

    @Test fun uturnKeepMerge() {
        assertEquals(P.M_UTURN_RIGHT, P.maneuver("Make a U-turn"))
        assertEquals(P.M_UTURN_LEFT, P.maneuver("Make a U-turn to the left"))
        assertEquals(P.M_FORK_LEFT, P.maneuver("Keep left to stay on NH65"))
        assertEquals(P.M_FORK_RIGHT, P.maneuver("Keep right at the fork"))
        assertEquals(P.M_MERGE, P.maneuver("Merge onto ORR"))
        assertEquals(P.M_RAMP_LEFT, P.maneuver("Take exit 2 on the left toward Shamshabad"))
        assertEquals(P.M_SHARP_RIGHT, P.maneuver("Sharp right onto Lane 4"))
        assertEquals(P.M_STRAIGHT, P.maneuver("Head north on Road No. 1"))
        assertEquals(P.M_STRAIGHT, P.maneuver("Continue onto Biodiversity Flyover"))
    }

    @Test fun arrived() {
        val g = P.parse("Charminar", "You have arrived at your destination", "", now)!!
        assertTrue(g.arrived)
        assertEquals(P.M_ARRIVE, g.maneuver)
    }

    @Test fun headTowardHasNoDistance() {
        val g = P.parse("Head toward Durgam Cheruvu Rd", "", "8 min · 2.1 km", now)!!
        assertNull(g.toTurnM)
        assertEquals(2100, g.remainingM)
        assertEquals("Durgam Cheruvu Rd", g.road)
        assertNotNull(g.eta)
    }

    @Test fun emptyIsNull() = assertNull(P.parse("", "", "", now))

    /** Real Motorola Razr 60 / Android 15 notification: road only, the direction is in the icon. */
    @Test fun razrRoadOnlyUsesIcon() {
        val g = P.parse("170 m", "W Jubilee Hills Rd", "35 min · 22 km · 06:39 ETA", now, P.M_LEFT)!!
        assertEquals(P.M_LEFT, g.maneuver)
        assertEquals(170, g.toTurnM)
        assertEquals(22000, g.remainingM)
        assertEquals("W Jubilee Hills Rd", g.road)
        assertEquals(LocalTime.of(6, 39), g.eta)
    }

    @Test fun razrTowards() {
        val g = P.parse("0 m", "towards 3rd St", "35 min · 22 km · 06:54 ETA", now, P.M_STRAIGHT)!!
        assertEquals("3rd St", g.road)
        assertEquals(P.M_STRAIGHT, g.maneuver)
    }

    @Test fun reroutingIsAStatusNotARoad() {
        val g = P.parse("", "Rerouting...", "36 min · 22 km · 07:17 ETA", now, P.M_STRAIGHT)!!
        assertTrue(g.rerouting)
        assertEquals("", g.road)
        assertEquals(22000, g.remainingM)
        val s = P.parse("Starting navigation…", "", "", now)!!
        assertEquals("", s.road)
    }

    @Test fun wordsBeatIcon() {
        val g = P.parse("250 m", "Turn right onto Old Mumbai Hwy", "", now, P.M_LEFT)!!
        assertEquals(P.M_RIGHT, g.maneuver)
    }
}
