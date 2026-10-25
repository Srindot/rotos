package dev.rotos.app.ui

import dev.rotos.app.link.LinkState
import dev.rotos.app.mirror.Guidance
import dev.rotos.app.mirror.GuidanceBus
import dev.rotos.app.nav.Packets
import dev.rotos.app.ride.RideStatus
import dev.rotos.app.route.AreaRoute

/**
 * RideStatus -> what each card on the main screen shows. Pure (no Android
 * views), so the wording lives in one place.
 */
data class UiModel(
    val phase: String,
    val link: LinkState,
    /** The ride got past "connecting" at least once (CONNECTING now = reconnecting). */
    val wasConnected: Boolean,
    val progress: Progress?,
    val error: String?,
    val routeSource: String,
    val routeKm: String,
    val routeMin: String,
    val routeTurns: String,
    val routeNote: String,
    val ride: Ride?,
) {
    data class Progress(val title: String, val detail: String, val pct: Int?)
    data class Ride(val title: String, val kmLeft: String, val eta: String, val speed: String, val flags: String)

    companion object {
        private val ACTIVE = setOf("connecting", "planning", "planned", "sending map", "riding", "arrived", "ride finished")

        /** A ride is running (the big button reads "Stop ride"). */
        fun isActive(phase: String): Boolean = phase in ACTIVE

        fun from(s: RideStatus): UiModel {
            val plan = s.plan
            val area = plan?.route?.source == "area"
            val g = GuidanceBus.now.value
            return UiModel(
                phase = s.phase,
                link = s.link,
                wasConnected = s.peer.isNotEmpty(),                          // set on the first connect
                progress = progress(s),
                error = s.error.takeIf { s.phase == "error" && it.isNotEmpty() }?.replaceFirstChar { it.uppercase() },
                routeSource = if (area) "Google Maps navigates" else plan?.route?.source?.let(::sourceName) ?: "",
                routeKm = when {
                    area -> g?.remainingM?.let { "%.1f".format(it / 1000f) } ?: "—"
                    else -> plan?.let { "%.1f".format(it.route.totalM / 1000f) } ?: ""
                },
                routeMin = when {
                    area -> g?.eta?.let { "%02d:%02d".format(it.hour, it.minute) } ?: "—"
                    else -> plan?.let { "%d".format(((it.route.durationS + 59f) / 60f).toInt()) } ?: ""
                },
                routeTurns = when {
                    area -> g?.toTurnM?.let { "$it m" } ?: "—"
                    else -> plan?.route?.maneuvers?.size?.toString() ?: ""
                },
                routeNote = when {
                    area -> "ROTOS map: the %.0f × %.0f km around you (%d KB). Google's route is not shared with other apps, so ROTOS shows the map and Google's turns."
                        .format(AreaRoute.HALF_KM * 2, AreaRoute.HALF_KM * 2, (plan?.pack?.length ?: 0) / 1024)
                    else -> plan?.let { "Map for ROTOS: %d KB · flyovers: %s".format(it.pack.length / 1024, it.pack.flyover) } ?: ""
                },
                ride = ride(s),
            )
        }

        private val TURN_WORDS = mapOf(
            2 to "straight", 3 to "slight left", 4 to "left", 5 to "sharp left", 6 to "U-turn",
            7 to "slight right", 8 to "right", 9 to "sharp right", 10 to "U-turn", 11 to "roundabout",
            12 to "roundabout", 13 to "keep left", 14 to "keep right", 15 to "merge", 16 to "exit left",
            17 to "exit right", 19 to "arrive",
        )

        /** "right in 250 m onto Old Mumbai Hwy · arrive 17:58" */
        fun describe(g: Guidance): String {
            if (g.arrived) return "arrived"
            val turn = TURN_WORDS[g.maneuver] ?: "continue"
            val dist = g.toTurnM?.let { if (it >= 1000) " in %.1f km".format(it / 1000f) else " in $it m" }.orEmpty()
            val road = if (g.road.isNotEmpty()) " onto ${g.road}" else ""
            val eta = g.eta?.let { " · arrive %02d:%02d".format(it.hour, it.minute) }.orEmpty()
            return turn + dist + road + eta
        }

        private fun sourceName(src: String): String = when {
            src.startsWith("google") -> "Google · two-wheeler"
            src.contains("sample", ignoreCase = true) -> "Sample"
            else -> src
        }

        private fun progress(s: RideStatus): Progress? {
            val t = s.transfer
            if (s.phase == "sending map" && t != null && t.total > 0) {
                val pct = (t.acked * 100L / t.total).toInt()
                return Progress("Sending map to ROTOS", "${t.acked / 1024} of ${t.total / 1024} KB", pct)
            }
            if (s.phase == "sending map") return Progress("Sending map to ROTOS", "", null)
            if (s.phase == "connecting") return Progress("Connecting to ROTOS", "Make sure ROTOS is powered on", null)
            if (s.phase != "planning") return null
            val b = s.build ?: return Progress("Planning the route", "", null)
            return when (b.prep) {
                Packets.PREP_ROUTE -> Progress("Finding the route", b.stage.substringAfter('(').substringBefore(')'), null)
                Packets.PREP_BRIDGES -> Progress("Checking flyovers", "Road names along the route", b.pct)
                Packets.PREP_MAP -> Progress(
                    "Building the map for ROTOS",
                    "Block ${b.done} of ${b.total}" + if (b.tiles > 0) " · downloading ${b.tiles} new map tiles (saved for next time)" else "",
                    b.pct,
                )
                else -> Progress("Map ready", "", 100)
            }
        }

        private fun ride(s: RideStatus): Ride? {
            val n = s.nav ?: return null
            if (s.phase !in setOf("riding", "arrived", "ride finished")) return null
            val arrived = n.flags and Packets.NAV_ARRIVED != 0
            val flags = listOfNotNull(
                if (n.flags and Packets.NAV_GPS_OK != 0) "GPS ok" else "Waiting for GPS",
                "Off route".takeIf { n.flags and Packets.NAV_OFF_ROUTE != 0 },
                "Rerouting…".takeIf { n.flags and Packets.NAV_REROUTING != 0 },
                "ROTOS reconnecting…".takeIf { s.link == LinkState.CONNECTING },
            ).joinToString(" · ")
            return Ride(
                title = if (arrived) "You have arrived" else "Riding",
                kmLeft = "%.1f".format(n.remainingM / 1000f),
                eta = if (arrived) "—" else n.eta,
                speed = "%.0f".format(n.speedMps * 3.6f),
                flags = flags,
            )
        }
    }
}
