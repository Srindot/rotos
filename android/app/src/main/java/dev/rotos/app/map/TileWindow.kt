package dev.rotos.app.map

import android.graphics.BitmapFactory

/**
 * Stitches an RGB window (3 bytes per pixel, row-major) out of 256 px tiles
 * at one zoom, from disk only: downloads happen before (MapPackBuilder asks
 * TilePlan which tiles are worth one). Tiles not on disk are plain land (the
 * cells under them are not marked right, see TilePlan); undecodable ones too,
 * counted in [failed]. Keeps a small LRU of decoded tiles because
 * neighbouring blocks share their halo tiles. Blocking: call on Dispatchers.IO.
 */
class TileWindow(private val tiles: TileCache, private val zoom: Int) {
    private companion object {
        const val TILE = 256
        const val LRU_TILES = 48
        const val LAND = 0xF2EFE9                              // GoogleTiles' land colour
    }

    private val lru = object : LinkedHashMap<Long, IntArray>(LRU_TILES, 0.75f, true) {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<Long, IntArray>?) = size > LRU_TILES
    }

    var failed: Int = 0
        private set

    /** Tiles whose real pixels went into the last [rgb] window: only cells over these are right. */
    var lastUsed: Set<Pair<Int, Int>> = emptySet()
        private set

    /** World pixel (x0, y0) at [zoom], size w x h -> RGB bytes. */
    fun rgb(x0: Int, y0: Int, w: Int, h: Int): ByteArray {
        val out = ByteArray(w * h * 3)
        for (i in 0 until w * h) {
            out[3 * i] = (LAND shr 16).toByte()
            out[3 * i + 1] = (LAND shr 8).toByte()
            out[3 * i + 2] = LAND.toByte()
        }
        val tx0 = Math.floorDiv(x0, TILE)
        val ty0 = Math.floorDiv(y0, TILE)
        val tx1 = Math.floorDiv(x0 + w - 1, TILE)
        val ty1 = Math.floorDiv(y0 + h - 1, TILE)
        val used = HashSet<Pair<Int, Int>>()
        for (ty in ty0..ty1) {
            for (tx in tx0..tx1) {
                val px = pixels(tx, ty) ?: continue
                blit(px, tx * TILE, ty * TILE, x0, y0, w, h, out)
                used.add(tx to ty)
            }
        }
        lastUsed = used
        return out
    }

    private fun blit(px: IntArray, ox: Int, oy: Int, x0: Int, y0: Int, w: Int, h: Int, out: ByteArray) {
        val xa = maxOf(ox, x0)
        val xb = minOf(ox + TILE, x0 + w)
        val ya = maxOf(oy, y0)
        val yb = minOf(oy + TILE, y0 + h)
        for (y in ya until yb) {
            var o = ((y - y0) * w + (xa - x0)) * 3
            var i = (y - oy) * TILE + (xa - ox)
            for (x in xa until xb) {
                val c = px[i++]
                out[o++] = (c shr 16).toByte()
                out[o++] = (c shr 8).toByte()
                out[o++] = c.toByte()
            }
        }
    }

    private fun pixels(tx: Int, ty: Int): IntArray? {
        val n = 1 shl zoom
        if (tx < 0 || ty < 0 || tx >= n || ty >= n) return null
        val key = (tx.toLong() shl 32) or (ty.toLong() and 0xFFFFFFFFL)
        lru[key]?.let { return it }
        val png = tiles.read(zoom, tx, ty) ?: return null      // not on disk (yet): never remembered as missing
        val px = decode(png)
        if (px == null) {
            failed++
            tiles.forget(zoom, tx, ty)                          // undecodable: downloaded again next time
            return null
        }
        lru[key] = px
        return px
    }

    private fun decode(png: ByteArray): IntArray? {
        val bmp = BitmapFactory.decodeByteArray(png, 0, png.size) ?: return null
        if (bmp.width != TILE || bmp.height != TILE) {
            bmp.recycle()
            return null
        }
        val px = IntArray(TILE * TILE)
        bmp.getPixels(px, 0, TILE, 0, 0, TILE, TILE)
        bmp.recycle()
        return px
    }
}
