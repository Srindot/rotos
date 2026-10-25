package dev.rotos.app.map

import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.File
import java.util.zip.Deflater
import java.util.zip.Inflater

/**
 * Converted map blocks on disk (filesDir, so Android does not clear them):
 * a block's classes depend only on Google's tiles under it, and blocks sit on
 * the global 256 px grid at zoom 16, so every later map over the same streets
 * reuses them - no tile reading, no classifier - whatever route Google picks
 * through them. Each block keeps a mask of the cells that were built from
 * real tiles (TilePlan.rightCells); it serves a map only if that mask covers
 * the cells the map needs, and later builds add cells to it.
 * File: u64 mask (big-endian), then the deflated classes. Per style and converter version.
 */
class BlockCache(filesDir: File) {
    companion object {
        /** Bump when rn_mapgen's output for the same tiles, or the file format, changes. */
        const val CONVERTER_VERSION = 2
        /** ~8 KB each: ~40 MB at most (a few hundred km of routes). */
        const val MAX_BLOCKS = 5_000
        private const val OUT = MapPackBuilder.OUT
    }

    private val dir = File(filesDir, "mapblocks/v${GoogleTiles.STYLE_VERSION}.$CONVERTER_VERSION")

    init {
        dir.mkdirs()
        prune()
    }

    private fun file(gx: Int, gy: Int) = File(dir, "${gx}_$gy.cls")

    /** The block's cell mask (0 when not saved). Reads 8 bytes only. */
    fun mask(gx: Int, gy: Int): Long = runCatching {
        DataInputStream(file(gx, gy).inputStream()).use { it.readLong() }
    }.getOrDefault(0L)

    /** The block's classes if its right cells cover [need], else null. */
    fun get(gx: Int, gy: Int, need: Long): ByteArray? = runCatching {
        val f = file(gx, gy)
        if (!f.isFile) return null
        val bytes = f.readBytes()
        val mask = DataInputStream(bytes.inputStream()).use { it.readLong() }
        if (mask and need != need) return null
        inflate(bytes, 8, OUT * OUT)
    }.getOrNull()

    /** Saves [classes] whose [right] cells are valid, keeping cells an earlier save had right. */
    fun put(gx: Int, gy: Int, classes: ByteArray, right: Long) {
        if (right == 0L || classes.size != OUT * OUT) return
        runCatching {
            val old = file(gx, gy).takeIf { it.isFile }?.readBytes()
            val oldMask = old?.let { DataInputStream(it.inputStream()).use { s -> s.readLong() } } ?: 0L
            val extra = oldMask and right.inv()
            if (extra == 0L && oldMask == right) return                // nothing new
            val merged = classes.copyOf()
            if (extra != 0L) inflate(old!!, 8, OUT * OUT)?.let { copyCells(it, merged, extra) } ?: return
            val bos = ByteArrayOutputStream(OUT * OUT / 8)
            java.io.DataOutputStream(bos).writeLong(right or extra)
            bos.write(deflate(merged))
            val tmp = File(dir, "${gx}_$gy.${Thread.currentThread().id}.part")
            tmp.writeBytes(bos.toByteArray())
            if (!tmp.renameTo(file(gx, gy))) tmp.delete()
        }
    }

    /** Copies the [cells] (mask) of block classes [from] into [to] (halo rows / columns excluded). */
    private fun copyCells(from: ByteArray, to: ByteArray, cells: Long) {
        val h = MapPackBuilder.HALO
        val c = MapPackBuilder.CELL
        for (i in 0 until 64) {
            if (cells and (1L shl i) == 0L) continue
            val x0 = h + (i % 8) * c
            val y0 = h + (i / 8) * c
            for (row in 0 until c) System.arraycopy(from, (y0 + row) * OUT + x0, to, (y0 + row) * OUT + x0, c)
        }
    }

    private fun deflate(data: ByteArray): ByteArray {
        val def = Deflater(Deflater.BEST_SPEED)
        try {
            def.setInput(data)
            def.finish()
            val bos = ByteArrayOutputStream(data.size / 8)
            val buf = ByteArray(8192)
            while (!def.finished()) bos.write(buf, 0, def.deflate(buf))
            return bos.toByteArray()
        } finally {
            def.end()
        }
    }

    private fun inflate(bytes: ByteArray, offset: Int, size: Int): ByteArray? {
        val inf = Inflater()
        try {
            inf.setInput(bytes, offset, bytes.size - offset)
            val out = ByteArray(size)
            var n = 0
            while (n < size && !inf.finished()) {
                val k = inf.inflate(out, n, size - n)
                if (k == 0 && (inf.needsInput() || inf.needsDictionary())) break
                n += k
            }
            return out.takeIf { n == size && inf.finished() }
        } finally {
            inf.end()
        }
    }

    /** Over [MAX_BLOCKS]: drop the oldest; older versions' folders go entirely. */
    private fun prune() {
        dir.parentFile?.listFiles()?.filter { it != dir }?.forEach { it.deleteRecursively() }
        val files = dir.listFiles() ?: return
        if (files.size <= MAX_BLOCKS) return
        files.sortedBy { it.lastModified() }.take(files.size - MAX_BLOCKS).forEach { it.delete() }
    }
}
