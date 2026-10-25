package dev.rotos.app.map

import dev.rotos.app.route.LatLon

/**
 * Which Google tiles a map really needs - the frugal part of map building.
 * Pure (no I/O): MapPackBuilder and its background warming both plan with it.
 *
 * Units: a *cell* is 32 px at zoom 16 (one ROTOS map tile), a *block* is
 * 8 x 8 cells on the global grid (what the converter works on, with a
 * [HALO] px margin), a *tile* is Google's 256 px tile at zoom 17.
 *
 *   1. Only cells the map keeps count, never whole blocks: a corridor that
 *      clips a block's corner needs the 1-4 tiles under that corner, not all
 *      16 under the block's window (41 % fewer tiles on a real 22 km route).
 *   2. A cell is right once every tile under it and its [HALO] is real (the
 *      converter's filters reach no further), so blocks are saved with a
 *      64-bit mask of their right cells and reused for any later map whose
 *      cells that mask covers; anything else in a block window is filled
 *      with land, never downloaded.
 */
object TilePlan {
    const val CELL = MapPackBuilder.CELL
    const val BLOCK_CELLS = MapPackBuilder.BLOCK_CELLS
    const val HALO = MapPackBuilder.HALO
    private const val TILE = 256

    /** Bit of local cell (lx, ly) in a block's 64-cell mask. */
    fun bit(lx: Int, ly: Int): Long = 1L shl (ly * BLOCK_CELLS + lx)

    /** Global cells -> (global block -> mask of those cells), in first-seen order. */
    fun byBlock(cells: Iterable<Pair<Int, Int>>): LinkedHashMap<Pair<Int, Int>, Long> {
        val out = LinkedHashMap<Pair<Int, Int>, Long>()
        for ((cx, cy) in cells) {
            val b = Math.floorDiv(cx, BLOCK_CELLS) to Math.floorDiv(cy, BLOCK_CELLS)
            out[b] = (out[b] ?: 0L) or bit(Math.floorMod(cx, BLOCK_CELLS), Math.floorMod(cy, BLOCK_CELLS))
        }
        return out
    }

    /** Zoom-17 tiles under global cell (cx, cy) and its halo. */
    fun tilesOfCell(cx: Int, cy: Int): List<Pair<Int, Int>> {
        val x0 = 2 * (cx * CELL - HALO)
        val y0 = 2 * (cy * CELL - HALO)
        val x1 = 2 * (cx * CELL + CELL + HALO) - 1
        val y1 = 2 * (cy * CELL + CELL + HALO) - 1
        val out = ArrayList<Pair<Int, Int>>(4)
        for (ty in Math.floorDiv(y0, TILE)..Math.floorDiv(y1, TILE)) {
            for (tx in Math.floorDiv(x0, TILE)..Math.floorDiv(x1, TILE)) out.add(tx to ty)
        }
        return out
    }

    /** The tiles the cells of [need] (block -> cell mask) really need. */
    fun tilesFor(need: Map<Pair<Int, Int>, Long>): LinkedHashSet<Pair<Int, Int>> {
        val out = LinkedHashSet<Pair<Int, Int>>()
        forEachCell(need) { cx, cy -> out.addAll(tilesOfCell(cx, cy)) }
        return out
    }

    /** Cells of block (gx, gy) whose tiles (halo included) are all [present]. */
    fun rightCells(gx: Int, gy: Int, present: (Int, Int) -> Boolean): Long {
        var mask = 0L
        for (ly in 0 until BLOCK_CELLS) {
            for (lx in 0 until BLOCK_CELLS) {
                val ok = tilesOfCell(gx * BLOCK_CELLS + lx, gy * BLOCK_CELLS + ly).all { (tx, ty) -> present(tx, ty) }
                if (ok) mask = mask or bit(lx, ly)
            }
        }
        return mask
    }

    /** Global cells within [radiusPx] (zoom 16) of the polyline, in order along it. */
    fun cellsAround(points: List<LatLon>, radiusPx: Int): LinkedHashSet<Pair<Int, Int>> {
        val out = LinkedHashSet<Pair<Int, Int>>()
        val px = points.map { MapPackBuilder.worldPx(it) }
        val r2 = radiusPx.toDouble() * radiusPx
        for (i in px.indices) {
            val (ax, ay) = px[i]
            val (bx, by) = if (i + 1 < px.size) px[i + 1] else px[i]
            val steps = maxOf(1, (Math.hypot(bx - ax, by - ay) / (CELL / 2)).toInt())
            for (k in 0..steps) {
                val x = ax + (bx - ax) * k / steps
                val y = ay + (by - ay) * k / steps
                for (cy in Math.floorDiv((y - radiusPx).toInt(), CELL)..Math.floorDiv((y + radiusPx).toInt(), CELL)) {
                    for (cx in Math.floorDiv((x - radiusPx).toInt(), CELL)..Math.floorDiv((x + radiusPx).toInt(), CELL)) {
                        val dx = maxOf(cx * CELL - x, 0.0, x - (cx + 1) * CELL)
                        val dy = maxOf(cy * CELL - y, 0.0, y - (cy + 1) * CELL)
                        if (dx * dx + dy * dy <= r2) out.add(cx to cy)
                    }
                }
            }
        }
        return out
    }

    private inline fun forEachCell(need: Map<Pair<Int, Int>, Long>, f: (Int, Int) -> Unit) {
        for ((b, mask) in need) {
            for (i in 0 until BLOCK_CELLS * BLOCK_CELLS) {
                if (mask and (1L shl i) != 0L) f(b.first * BLOCK_CELLS + i % BLOCK_CELLS, b.second * BLOCK_CELLS + i / BLOCK_CELLS)
            }
        }
    }
}
