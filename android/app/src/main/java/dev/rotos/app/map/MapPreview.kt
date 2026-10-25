package dev.rotos.app.map

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.route.LatLon
import kotlin.math.ceil

/**
 * Renders the current map pack (as stored, read back through
 * rn_ffi_pack_class_at) in the device's night palette, plus the route line.
 * Call [render] on NavThread.
 */
object MapPreview {
    /** Night theme, class order of rn_mappack.h (land .. road_major). */
    val NIGHT = intArrayOf(
        0xFF242F3E.toInt(), 0xFF1F2835.toInt(), 0xFF263C3F.toInt(), 0xFF17263C.toInt(),     // ROTOS night =
        0xFF2C3A4B.toInt(), 0xFF111821.toInt(), 0xFF4A5568.toInt(), 0xFF9A8460.toInt(),     // Google dark map
    )
    private const val OUTSIDE = 0xFF000000.toInt()
    private const val MAX_SIDE = 1200

    fun render(info: PackInfo, route: List<LatLon>): Bitmap {
        val wPx = info.tilesW * MapPackBuilder.CELL
        val hPx = info.tilesH * MapPackBuilder.CELL
        val step = maxOf(1, ceil(maxOf(wPx, hPx) / MAX_SIDE.toDouble()).toInt())
        val w = minOf(4096, wPx / step)
        val h = minOf(4096, hPx / step)
        val px = IntArray(w * h)
        val row = ByteArray(w)
        for (y in 0 until h) {
            NativeNav.packClassRow(0, y * step, step, row, w)
            for (x in 0 until w) {
                val c = row[x].toInt()
                px[y * w + x] = if (c in 0..7) NIGHT[c] else OUTSIDE
            }
        }
        val bmp = Bitmap.createBitmap(px, w, h, Bitmap.Config.ARGB_8888).copy(Bitmap.Config.ARGB_8888, true)
        drawRoute(Canvas(bmp), route.size, step)
        return bmp
    }

    private fun drawRoute(c: Canvas, n: Int, step: Int) {
        if (n < 2) return
        val p = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = Color.rgb(0x42, 0x85, 0xF4)                 // Google blue, like ROTOS
            strokeWidth = maxOf(2f, 5f / step)
            style = Paint.Style.STROKE
            strokeCap = Paint.Cap.ROUND
        }
        val xy = FloatArray(2)
        var prev: Pair<Float, Float>? = null
        for (i in 0 until minOf(n, 4096)) {
            // Pack pixels straight from the library (rn_ffi_corridor_point_px).
            if (NativeNav.corridorPointPx(i, xy) < 0) continue
            val x = xy[0] / step
            val y = xy[1] / step
            prev?.let { c.drawLine(it.first, it.second, x, y, p) }
            prev = x to y
        }
    }
}
