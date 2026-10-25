package dev.rotos.app.mirror

import dev.rotos.app.mirror.GuidanceParser as P
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import kotlin.math.hypot

/** Arrows drawn like Google's: a thick stem from the bottom, a triangular head. */
class ArrowClassifierTest {
    private val n = 64
    private val white = 0xFFFFFFFF.toInt()

    private fun canvas() = IntArray(n * n)

    private fun line(px: IntArray, x0: Double, y0: Double, x1: Double, y1: Double, r: Double = 4.0) {
        for (y in 0 until n) for (x in 0 until n) {
            val dx = x1 - x0; val dy = y1 - y0
            val t = (((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy)).coerceIn(0.0, 1.0)
            if (hypot(x - (x0 + t * dx), y - (y0 + t * dy)) <= r) px[y * n + x] = white
        }
    }

    /** Arrowhead at (x, y) pointing along (ux, uy). */
    private fun head(px: IntArray, x: Double, y: Double, ux: Double, uy: Double) {
        val s = 12.0
        val bx = x - ux * s; val by = y - uy * s
        val ax = bx - uy * s * 0.8; val ay = by + ux * s * 0.8
        val cx = bx + uy * s * 0.8; val cy = by - ux * s * 0.8
        for (yy in 0 until n) for (xx in 0 until n) {
            fun side(x1: Double, y1: Double, x2: Double, y2: Double) = (x2 - x1) * (yy - y1) - (y2 - y1) * (xx - x1)
            val d1 = side(x, y, ax, ay); val d2 = side(ax, ay, cx, cy); val d3 = side(cx, cy, x, y)
            val neg = d1 < 0 || d2 < 0 || d3 < 0
            val pos = d1 > 0 || d2 > 0 || d3 > 0
            if (!(neg && pos)) px[yy * n + xx] = white
        }
    }

    @Test fun straight() {
        val px = canvas(); line(px, 32.0, 60.0, 32.0, 18.0); head(px, 32.0, 4.0, 0.0, -1.0)
        assertEquals(P.M_STRAIGHT, ArrowClassifier.classify(px, n, n))
    }

    @Test fun turnLeftAndRight() {
        val l = canvas(); line(l, 40.0, 60.0, 40.0, 26.0); line(l, 40.0, 26.0, 18.0, 26.0); head(l, 4.0, 26.0, -1.0, 0.0)
        assertEquals(P.M_LEFT, ArrowClassifier.classify(l, n, n))
        val r = canvas(); line(r, 24.0, 60.0, 24.0, 26.0); line(r, 24.0, 26.0, 46.0, 26.0); head(r, 60.0, 26.0, 1.0, 0.0)
        assertEquals(P.M_RIGHT, ArrowClassifier.classify(r, n, n))
    }

    @Test fun slightLeft() {
        val px = canvas(); line(px, 40.0, 60.0, 40.0, 36.0); line(px, 40.0, 36.0, 22.0, 18.0); head(px, 12.0, 8.0, -0.707, -0.707)
        assertEquals(P.M_SLIGHT_LEFT, ArrowClassifier.classify(px, n, n))
    }

    @Test fun uTurn() {
        val px = canvas(); line(px, 44.0, 60.0, 44.0, 14.0); line(px, 44.0, 14.0, 20.0, 14.0); line(px, 20.0, 14.0, 20.0, 40.0)
        head(px, 20.0, 56.0, 0.0, 1.0)
        assertEquals(P.M_UTURN_LEFT, ArrowClassifier.classify(px, n, n))
    }

    @Test fun roundabout() {
        val px = canvas()
        for (y in 0 until n) for (x in 0 until n) {
            val d = hypot(x - 32.0, y - 34.0)
            if (d in 9.0..14.0) px[y * n + x] = white
        }
        line(px, 32.0, 62.0, 32.0, 48.0); line(px, 44.0, 26.0, 52.0, 16.0); head(px, 60.0, 6.0, 0.707, -0.707)
        assertEquals(P.M_ROUNDABOUT_RIGHT, ArrowClassifier.classify(px, n, n))
    }

    /** Google Maps' real "head straight" icon from the Razr 60 (dashed tail), as a #/. alpha grid. */
    @Test fun realGoogleStraightDashed() {
        val rows = javaClass.getResourceAsStream("/arrows/razr60_straight_dashed.txt")!!.bufferedReader().readLines()
            .filter { it.isNotEmpty() }
        val w = rows[0].length
        val h = rows.size
        val px = IntArray(w * h) { if (rows[it / w][it % w] == '#') white else 0 }
        assertEquals(P.M_STRAIGHT, ArrowClassifier.classify(px, w, h))
        assertEquals(0x000c0e2a08080800L, ArrowClassifier.hash(px, w, h))
    }

    @Test fun emptyIconIsNull() = assertNull(ArrowClassifier.classify(canvas(), n, n))

    @Test fun sameIconSameHash() {
        val a = canvas(); line(a, 32.0, 60.0, 32.0, 18.0); head(a, 32.0, 4.0, 0.0, -1.0)
        val b = a.copyOf()
        assertEquals(ArrowClassifier.hash(a, n, n), ArrowClassifier.hash(b, n, n))
    }
}
