package dev.rotos.app.ride

import dev.rotos.app.route.GoogleTarget
import dev.rotos.app.route.Route

/** Log lines about planning, as pure functions. */
object PlanLog {
    /** Which candidate route was picked to match Google Maps, and why. */
    fun picked(all: List<Route>, best: Int, t: GoogleTarget?): String {
        if (all.size <= 1) return "route: one candidate"
        val routes = all.joinToString(" / ") { "%.1f km %d min".format(it.distanceM / 1000f, (it.durationS / 60).toInt()) }
        val google = t?.let { g ->
            " vs Google %s km, roads %s".format(g.remainingM?.let { "%.1f".format(it / 1000f) } ?: "?", g.roads.take(3))
        } ?: ""
        return "route: picked %d of %d to match Google (%s)".format(best + 1, all.size, routes) + google
    }

    /** What was planned: the route, then the map pack. */
    fun planned(p: Plan): List<String> = listOf(
        "route ${p.route.source}: %.0f m, %d pts, %d maneuvers, id %08x".format(
            p.route.totalM, p.route.points.size, p.route.maneuvers.size, p.route.routeId),
        "map pack %d B, crc %08x, %d cells, %d blocks (%d saved), %s tiles new %d missing %d, flyover: %s".format(
            p.pack.length, p.pack.crc32, p.pack.keptCells, p.pack.blocks, p.pack.blocksSaved, p.pack.tileSource,
            p.pack.tilesDownloaded, p.pack.tilesMissing, p.pack.flyover),
    )
}
