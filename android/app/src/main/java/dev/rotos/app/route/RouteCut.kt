package dev.rotos.app.route

/**
 * The first [maxM] metres of a route, for the quick first map in an area seen
 * for the first time (a few dozen tiles instead of hundreds: on ROTOS in
 * seconds, the full map follows). Steps past the cut are dropped, the step
 * across it is cut inside its polyline. Pure.
 */
object RouteCut {
    fun first(route: Route, maxM: Int): Route {
        if (route.distanceM <= maxM) return route
        val steps = ArrayList<RouteStep>()
        var done = 0
        for (s in route.steps) {
            if (done >= maxM) break
            if (done + s.distanceM <= maxM) {
                steps.add(s)
                done += s.distanceM
                continue
            }
            val pts = Polyline.decode(s.encodedPolyline)
            val keep = ArrayList<LatLon>()
            var m = 0.0
            for ((i, p) in pts.withIndex()) {
                if (i > 0) m += AreaRoute.distanceToPath(pts[i - 1], listOf(p))
                keep.add(p)
                if (done + m >= maxM) break
            }
            if (keep.size >= 2) steps.add(s.copy(encodedPolyline = Polyline.encode(keep), distanceM = (maxM - done)))
            done = maxM
        }
        if (steps.isEmpty()) return route
        val share = done.toFloat() / route.distanceM.coerceAtLeast(1)
        return route.copy(steps = steps, distanceM = done, durationS = route.durationS * share)
    }
}
