package dev.rotos.app.ride

import dev.rotos.app.route.LatLon
import dev.rotos.app.route.ManeuverInfo
import dev.rotos.app.route.Polyline
import dev.rotos.app.route.RoadNames
import dev.rotos.app.route.Route

/**
 * The route line ROTOS draws, in the pixels of the map pack it sits on (zoom
 * 16 Web Mercator, relative to the pack origin), simplified to at most
 * RN_ROUTE_MAX_PTS points. The phone follows the rider along exactly these
 * points and sends them as ROUTE packets, so both sides agree to the metre.
 * Built in Kotlin from Google's polylines: no NavThread on the way. Pure.
 */
class RouteLine(
    val id: Int,
    val packRouteId: Int,
    /** World px (zoom 16) of the pack's origin: rider positions are taken relative to it. */
    val originX: Int,
    val originY: Int,
    val xs: FloatArray,
    val ys: FloatArray,
    val along: FloatArray,
    val mPerPx: Float,
    val durationS: Float,
    val maneuvers: List<ManeuverInfo>,
) {
    val size: Int get() = xs.size

    /** A position in this line's pack pixels. */
    fun pxOf(p: LatLon): Pair<Float, Float> =
        (worldX(p.lon) - originX).toFloat() to (worldY(p.lat) - originY).toFloat()
    val totalM: Float get() = if (along.isEmpty()) 0f else along.last()

    companion object {
        const val MAX_PTS = 1000
        const val ZOOM = 16
        private const val WORLD = 256.0 * (1 shl ZOOM)

        fun worldX(lon: Double) = (lon + 180.0) / 360.0 * WORLD
        fun worldY(lat: Double): Double {
            val s = Math.sin(Math.toRadians(lat.coerceIn(-85.0, 85.0)))
            return (0.5 - Math.log((1 + s) / (1 - s)) / (4 * Math.PI)) * WORLD
        }

        /** Metres per zoom-16 pixel at [lat]. */
        fun metresPerPx(lat: Double) = (156_543.03392 * Math.cos(Math.toRadians(lat)) / (1 shl ZOOM)).toFloat()

        /** Google route -> its points and turns (alongM from the start). */
        fun pointsOf(route: Route): Pair<List<LatLon>, List<ManeuverInfo>> {
            val pts = ArrayList<LatLon>()
            val turns = ArrayList<ManeuverInfo>()
            var along = 0f
            for (step in route.steps) {
                val p = Polyline.decode(step.encodedPolyline)
                turns.add(ManeuverInfo(0, 0, along, RoadNames.ofInstruction(step.instruction)))
                for ((k, q) in p.withIndex()) {
                    if (k == 0 && pts.isNotEmpty()) continue        // steps share their joints
                    pts.add(q)
                }
                along += step.distanceM
            }
            return pts to turns
        }

        /** [points] on the pack with origin (originX, originY) (world px at zoom 16). */
        fun build(
            id: Int, packRouteId: Int, points: List<LatLon>, maneuvers: List<ManeuverInfo>, durationS: Float,
            originX: Int, originY: Int,
        ): RouteLine? {
            if (points.size < 2) return null
            val mid = points[points.size / 2].lat
            val xs = DoubleArray(points.size) { worldX(points[it].lon) - originX }
            val ys = DoubleArray(points.size) { worldY(points[it].lat) - originY }
            val keep = simplify(xs, ys)
            val m = metresPerPx(mid)
            val ax = FloatArray(keep.size) { xs[keep[it]].toFloat() }
            val ay = FloatArray(keep.size) { ys[keep[it]].toFloat() }
            val al = FloatArray(keep.size)
            for (i in 1 until keep.size) {
                al[i] = al[i - 1] + Math.hypot((ax[i] - ax[i - 1]).toDouble(), (ay[i] - ay[i - 1]).toDouble()).toFloat() * m
            }
            return RouteLine(id, packRouteId, originX, originY, ax, ay, al, m, durationS, maneuvers)
        }

        /** Douglas-Peucker, tolerance raised until at most MAX_PTS points remain. Indices kept. */
        fun simplify(xs: DoubleArray, ys: DoubleArray): IntArray {
            var tol = 0.75
            for (attempt in 0 until 12) {
                val keep = BooleanArray(xs.size)
                keep[0] = true
                keep[xs.size - 1] = true
                mark(xs, ys, 0, xs.size - 1, tol, keep)
                val idx = keep.indices.filter { keep[it] }
                if (idx.size <= MAX_PTS) return idx.toIntArray()
                tol *= 1.6
            }
            return IntArray(MAX_PTS) { it * (xs.size - 1) / (MAX_PTS - 1) }      // never reached in practice
        }

        /** Iterative Douglas-Peucker over [a, b] (an explicit stack: bounded, no recursion). */
        private fun mark(xs: DoubleArray, ys: DoubleArray, a: Int, b: Int, tol: Double, keep: BooleanArray) {
            val stack = ArrayDeque<Pair<Int, Int>>()
            stack.addLast(a to b)
            while (stack.isNotEmpty()) {
                val (i, j) = stack.removeLast()
                if (j - i < 2) continue
                var best = -1
                var bestD = tol
                for (k in i + 1 until j) {
                    val d = segDist(xs[k], ys[k], xs[i], ys[i], xs[j], ys[j])
                    if (d > bestD) { bestD = d; best = k }
                }
                if (best < 0) continue
                keep[best] = true
                stack.addLast(i to best)
                stack.addLast(best to j)
            }
        }

        fun segDist(px: Double, py: Double, ax: Double, ay: Double, bx: Double, by: Double): Double {
            val dx = bx - ax
            val dy = by - ay
            val l2 = dx * dx + dy * dy
            val t = if (l2 <= 0) 0.0 else (((px - ax) * dx + (py - ay) * dy) / l2).coerceIn(0.0, 1.0)
            return Math.hypot(px - ax - t * dx, py - ay - t * dy)
        }
    }
}

/**
 * Where the rider is along a RouteLine (pixels in, metres out), with the
 * off-the-line hysteresis the C engine used to provide: more than OFF_M away
 * for OFF_FIXES trusted fixes in a row = wants a new route. Not thread-safe:
 * one per ride, used from the fix loop.
 */
class LineFollower(val line: RouteLine) {
    companion object {
        const val OFF_M = 45f
        const val BACK_ON_M = 25f
        const val OFF_FIXES = 3
        private const val WINDOW_BACK = 8
        private const val WINDOW_AHEAD = 80
        private const val LOST_M = 150f
        private const val AGAINST_DEG = 110.0
    }

    private var seg = 0
    private var offCount = 0

    /** Position (pack px) -> Along; [trusted]: moving with a good fix (RerouteRules.trusted). */
    fun follow(x: Float, y: Float, trusted: Boolean, courseDeg: Float? = null): Along {
        // moving: only segments going the rider's way (the far side of a U-turn on a divided road
        // is a few metres away but points back; matching it would skip the line ahead)
        var best = nearest(x, y, maxOf(0, seg - WINDOW_BACK), minOf(line.size - 1, seg + WINDOW_AHEAD), courseDeg)
        if (best.second == Float.MAX_VALUE) best = nearest(x, y, maxOf(0, seg - WINDOW_BACK), minOf(line.size - 1, seg + WINDOW_AHEAD), null)
        if (best.second * line.mPerPx > LOST_M) best = nearest(x, y, 0, line.size - 1, courseDeg)   // jumped: search all
        if (best.second == Float.MAX_VALUE) best = nearest(x, y, 0, line.size - 1, null)
        seg = best.first
        val offM = best.second * line.mPerPx
        offCount = when {
            !trusted -> offCount
            offM > OFF_M -> offCount + 1
            offM < BACK_ON_M -> 0
            else -> offCount
        }
        val alongM = alongOn(seg, x, y)
        val remaining = (line.totalM - alongM).coerceAtLeast(0f)
        val remainingS = if (line.totalM > 0f) (line.durationS * remaining / line.totalM).toInt() else 0
        return Along(alongM, remaining, remainingS, offCount >= OFF_FIXES)
    }

    /** (segment index, distance in px) of the closest segment in [from, to]. */
    private fun nearest(x: Float, y: Float, from: Int, to: Int, courseDeg: Float?): Pair<Int, Float> {
        var bestI = from
        var bestD = Float.MAX_VALUE
        for (i in from until to) {
            if (courseDeg != null && against(i, courseDeg)) continue
            val d = RouteLine.segDist(x.toDouble(), y.toDouble(), line.xs[i].toDouble(), line.ys[i].toDouble(),
                line.xs[i + 1].toDouble(), line.ys[i + 1].toDouble()).toFloat()
            if (d < bestD) { bestD = d; bestI = i }
        }
        return bestI to bestD
    }

    /** Segment i points more than [AGAINST_DEG] away from the course (pack px: x east, y south). */
    private fun against(i: Int, courseDeg: Float): Boolean {
        val dx = (line.xs[i + 1] - line.xs[i]).toDouble()
        val dy = (line.ys[i + 1] - line.ys[i]).toDouble()
        if (dx * dx + dy * dy < 1e-6) return false
        val bearing = Math.toDegrees(Math.atan2(dx, -dy))
        var diff = Math.abs(bearing - courseDeg) % 360.0
        if (diff > 180.0) diff = 360.0 - diff
        return diff > AGAINST_DEG
    }

    private fun alongOn(i: Int, x: Float, y: Float): Float {
        if (line.size < 2) return 0f
        val dx = line.xs[i + 1] - line.xs[i]
        val dy = line.ys[i + 1] - line.ys[i]
        val l2 = dx * dx + dy * dy
        val t = if (l2 <= 0f) 0f else (((x - line.xs[i]) * dx + (y - line.ys[i]) * dy) / l2).coerceIn(0f, 1f)
        return line.along[i] + t * (line.along[i + 1] - line.along[i])
    }
}
