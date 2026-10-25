package dev.rotos.app.mirror

import kotlin.math.abs
import kotlin.math.atan2

/**
 * Google Maps' turn arrow (the notification's large icon) -> RnManeuverType.
 * Pure: works on ARGB pixels, so it is unit-tested with drawn arrows.
 *
 * On Motorola's Android 15 the notification text is only "170 m" /
 * "W Jubilee Hills Rd": the direction exists only in this picture.
 *
 *   1. mask: icon pixels (opaque, or unlike the border colour when the icon
 *      has a solid background)
 *   2. base = bottom of the arrow's stem (lowest row, centre); tip = the mask
 *      pixel farthest from it *along the arrow's body* (not straight-line:
 *      an arrowhead's corners are as far as its point in a straight line)
 *   3. pointing direction = from the centroid of the pixels around the tip to
 *      the tip: up = straight, up-left = slight left, left = left, down-left =
 *      sharp left, down = U-turn (side from where the tip is)
 *   4. an enclosed hole (a ring) = roundabout, side from the exit tip
 */
object ArrowClassifier {
    /** Null when there is no usable arrow (empty or unreadable icon). */
    fun classify(argb: IntArray, w: Int, h: Int): Int? {
        if (w < 8 || h < 8 || argb.size < w * h) return null
        val on = mask(argb, w, h)
        val n = on.count { it }
        if (n < w * h / 50) return null
        var minX = w; var maxX = -1; var minY = h; var maxY = -1
        for (y in 0 until h) for (x in 0 until w) if (on[y * w + x]) {
            if (x < minX) minX = x
            if (x > maxX) maxX = x
            if (y < minY) minY = y
            if (y > maxY) maxY = y
        }
        val bw = maxX - minX + 1
        val bh = maxY - minY + 1
        val bottom = (minX..maxX).filter { on[maxY * w + it] }
        val baseX = bottom.average()
        val start = maxY * w + bottom.minByOrNull { abs(it - baseX) }!!
        val dist = pathDistances(dilate(on, w, h), w, h, start)          // bridges dashed tails
        var tip = start
        for (i in dist.indices) if (dist[i] != Int.MAX_VALUE && dist[i] > dist[tip]) tip = i
        val tipX = tip % w
        val tipY = tip / w
        // the head: pixels within ~a quarter of the arrow's size of the tip, along the body
        val headLen = (0.28 * maxOf(bw, bh) * 2).toInt()                 // path units: 2 per pixel
        var cx = 0.0; var cy = 0.0; var cn = 0
        for (i in dist.indices) if (dist[i] != Int.MAX_VALUE && dist[tip] - dist[i] <= headLen) {
            cx += i % w; cy += i / w; cn++
        }
        if (cn == 0) return null
        cx /= cn; cy /= cn
        val angle = Math.toDegrees(atan2(tipX - cx, cy - tipY))       // 0 = up, + = right, ±180 = down
        val left = if (abs(angle) > 150) tipX < baseX else angle < 0
        if (hasRing(on, w, h, minX, maxX, minY, maxY, bw * bh)) {
            return if (tipX < baseX) GuidanceParser.M_ROUNDABOUT_LEFT else GuidanceParser.M_ROUNDABOUT_RIGHT
        }
        val a = abs(angle)
        return when {
            a < 25 -> GuidanceParser.M_STRAIGHT
            a < 65 -> if (left) GuidanceParser.M_SLIGHT_LEFT else GuidanceParser.M_SLIGHT_RIGHT
            a < 115 -> if (left) GuidanceParser.M_LEFT else GuidanceParser.M_RIGHT
            a < 155 -> if (left) GuidanceParser.M_SHARP_LEFT else GuidanceParser.M_SHARP_RIGHT
            else -> if (left) GuidanceParser.M_UTURN_LEFT else GuidanceParser.M_UTURN_RIGHT
        }
    }

    /** 64-bit difference hash of the mask on a 9x8 grid: the same Google icon gives the same hash. */
    fun hash(argb: IntArray, w: Int, h: Int): Long {
        val on = mask(argb, w, h)
        val cells = Array(8) { DoubleArray(9) }
        for (gy in 0 until 8) for (gx in 0 until 9) {
            var c = 0; var t = 0
            for (y in gy * h / 8 until (gy + 1) * h / 8) for (x in gx * w / 9 until (gx + 1) * w / 9) {
                t++
                if (on[y * w + x]) c++
            }
            cells[gy][gx] = if (t > 0) c.toDouble() / t else 0.0
        }
        var bits = 0L
        for (gy in 0 until 8) for (gx in 0 until 8) {
            bits = bits shl 1
            if (cells[gy][gx] > cells[gy][gx + 1]) bits = bits or 1L
        }
        return bits
    }

    /** Grows the mask by 2 px, so the dashes of Google's "head straight" tail connect. */
    private fun dilate(on: BooleanArray, w: Int, h: Int): BooleanArray {
        val out = on.copyOf()
        for (y in 0 until h) for (x in 0 until w) if (on[y * w + x]) {
            for (dy in -2..2) for (dx in -2..2) {
                val nx = x + dx
                val ny = y + dy
                if (nx in 0 until w && ny in 0 until h) out[ny * w + nx] = true
            }
        }
        return out
    }

    /** Path length from [start] through the mask (2 per straight step, 3 per diagonal). */
    private fun pathDistances(on: BooleanArray, w: Int, h: Int, start: Int): IntArray {
        val dist = IntArray(w * h) { Int.MAX_VALUE }
        val queue = java.util.PriorityQueue<Long>()
        dist[start] = 0
        queue.add(start.toLong())
        while (queue.isNotEmpty()) {
            val top = queue.poll() ?: break
            val d = (top ushr 32).toInt()
            val i = (top and 0xFFFFFFFFL).toInt()
            if (d > dist[i]) continue
            val x = i % w
            val y = i / w
            for (dy in -1..1) for (dx in -1..1) {
                if (dx == 0 && dy == 0) continue
                val nx = x + dx
                val ny = y + dy
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue
                val j = ny * w + nx
                if (!on[j]) continue
                val nd = d + if (dx != 0 && dy != 0) 3 else 2
                if (nd < dist[j]) {
                    dist[j] = nd
                    queue.add((nd.toLong() shl 32) or j.toLong())
                }
            }
        }
        return dist
    }

    private fun mask(argb: IntArray, w: Int, h: Int): BooleanArray {
        val border = ArrayList<Int>()
        for (x in 0 until w) { border.add(argb[x]); border.add(argb[(h - 1) * w + x]) }
        for (y in 0 until h) { border.add(argb[y * w]); border.add(argb[y * w + w - 1]) }
        val transparent = border.count { (it ushr 24) < 64 } > border.size / 2
        if (transparent) return BooleanArray(w * h) { (argb[it] ushr 24) >= 128 }
        val br = border.map { (it shr 16) and 255 }.average()
        val bg = border.map { (it shr 8) and 255 }.average()
        val bb = border.map { it and 255 }.average()
        return BooleanArray(w * h) {
            val p = argb[it]
            abs(((p shr 16) and 255) - br) + abs(((p shr 8) and 255) - bg) + abs((p and 255) - bb) > 150
        }
    }

    /** A background region inside the arrow's box not connected to the box edge (a ring's hole). */
    private fun hasRing(on: BooleanArray, w: Int, h: Int, x0: Int, x1: Int, y0: Int, y1: Int, area: Int): Boolean {
        val seen = BooleanArray(w * h)
        val stack = ArrayDeque<Int>()
        for (x in x0..x1) { stack.add(y0 * w + x); stack.add(y1 * w + x) }
        for (y in y0..y1) { stack.add(y * w + x0); stack.add(y * w + x1) }
        while (stack.isNotEmpty()) {
            val i = stack.removeLast()
            if (seen[i] || on[i]) continue
            seen[i] = true
            val x = i % w
            val y = i / w
            if (x > x0) stack.add(i - 1)
            if (x < x1) stack.add(i + 1)
            if (y > y0) stack.add(i - w)
            if (y < y1) stack.add(i + w)
        }
        var hole = 0
        for (y in y0..y1) for (x in x0..x1) if (!on[y * w + x] && !seen[y * w + x]) hole++
        return hole > area / 40
    }
}
