package dev.rotos.app.ui

import dev.rotos.app.nav.Packets
import dev.rotos.app.ride.RideStatus

/** RideStatus -> the status text block. Pure. */
object StatusFormatter {
    fun format(s: RideStatus): String = buildString {
        appendLine("phase:    ${s.phase}")
        appendLine("link:     ${s.link}${if (s.peer.isNotEmpty()) "  " + s.peer else ""}")
        s.plan?.let { p ->
            appendLine("route:    %s, %.2f km, %d maneuvers, id %08x".format(
                p.route.source, p.route.totalM / 1000f, p.route.maneuvers.size, p.route.routeId))
            appendLine("map pack: %d KiB, crc %08x, %d cells".format(p.pack.length / 1024, p.pack.crc32, p.pack.keptCells))
        }
        s.build?.let { b -> if (s.phase == "planning") appendLine("build:    ${b.stage} ${b.done}/${b.total}") }
        s.transfer?.let { t ->
            appendLine("transfer: %d / %d B (%d%%), resends %d".format(
                t.acked, t.total, if (t.total > 0) t.acked * 100L / t.total else 0, t.resends))
        }
        s.nav?.let { n ->
            appendLine("nav:      fix %d, along %.0f m, left %.0f m, %d s, ETA %s".format(
                n.fixes, n.alongM, n.remainingM, n.remainingS, n.eta))
            appendLine("flags:    ${flags(n.flags)}  speed %.0f km/h".format(n.speedMps * 3.6f))
        }
        if (s.error.isNotEmpty()) appendLine("error:    ${s.error}")
    }.trimEnd()

    /** One friendly line + percent while planning or sending the map; null otherwise. */
    fun progressLine(s: RideStatus): Pair<String, Int>? {
        val t = s.transfer
        if (t != null && t.total > 0 && t.acked < t.total) {
            val pct = (t.acked * 100L / t.total).toInt()
            return "Sending map to ROTOS: ${t.acked / 1024} / ${t.total / 1024} KB" to pct
        }
        val b = s.build ?: return null
        if (s.phase != "planning") return null
        val text = when {
            b.total > 0 && b.stage.startsWith("downloading") -> "Downloading new map tiles: ${b.done} of ${b.total} (saved for next time)"
            b.total > 0 -> "Building map: block ${b.done} of ${b.total}"
            else -> b.stage.replaceFirstChar { it.uppercase() } + "…"
        }
        return text to b.pct
    }

    fun flags(f: Int): String = listOfNotNull(
        "GPS_OK".takeIf { f and Packets.NAV_GPS_OK != 0 },
        "OFF_ROUTE".takeIf { f and Packets.NAV_OFF_ROUTE != 0 },
        "REROUTING".takeIf { f and Packets.NAV_REROUTING != 0 },
        "NIGHT".takeIf { f and Packets.NAV_NIGHT != 0 },
        "ARRIVED".takeIf { f and Packets.NAV_ARRIVED != 0 },
    ).joinToString(" ").ifEmpty { "-" }
}
