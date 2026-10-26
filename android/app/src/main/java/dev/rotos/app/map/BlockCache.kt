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
        private const val UPKEEP_MS = 3_600_000L
        /** Never pruned: blocks used this recently (a build may have just chosen them). */
        private const val KEEP_RECENT_MS = 3_600_000L
        private val LOCKS = Array(64) { Any() }
        private val shared = java.util.concurrent.ConcurrentHashMap<String, BlockCache>()

        /** The app's one cache for [filesDir] (builds and warming share its upkeep). */
        fun of(filesDir: File): BlockCache = shared.getOrPut(filesDir.absolutePath) { BlockCache(filesDir) }
    }

    @Volatile private var upkeptMs = 0L

    private val dir = File(filesDir, "mapblocks/v${GoogleTiles.STYLE_VERSION}.$CONVERTER_VERSION")

    /** Folder and pruning, at most hourly (a disk walk: callers are on Dispatchers.IO, never the UI thread). */
    private val ready: Boolean
        get() {
            val now = System.currentTimeMillis()
            if (now - upkeptMs >= UPKEEP_MS) {
                synchronized(this) {
                    if (now - upkeptMs >= UPKEEP_MS) {
                        dir.mkdirs()
                        prune(now)
                        upkeptMs = now
                    }
                }
            }
            return true
        }

    private fun touch(f: File) {
        val now = System.currentTimeMillis()
        if (now - f.lastModified() > KEEP_RECENT_MS / 2) f.setLastModified(now)   // used: pruning keeps it this hour
    }

    private fun file(gx: Int, gy: Int) = File(dir, "${gx}_$gy.cls")

    /** The block's cell mask (0 when not saved). Reads 8 bytes only. */
    fun mask(gx: Int, gy: Int): Long = runCatching {
        check(ready)
        val f = file(gx, gy)
        DataInputStream(f.inputStream()).use { it.readLong() }.also { touch(f) }   // chosen for a map: keep it
    }.getOrDefault(0L)

    /** The block's classes if its right cells cover [need], else null. */
    fun get(gx: Int, gy: Int, need: Long): ByteArray? = runCatching {
        check(ready)
        val f = file(gx, gy)
        if (!f.isFile) return null
        touch(f)
        val bytes = f.readBytes()
        val mask = DataInputStream(bytes.inputStream()).use { it.readLong() }
        if (mask and need != need) return null
        inflate(bytes, 8, OUT * OUT)
    }.getOrNull()

    /** Saves [classes] whose [right] cells are valid, keeping cells an earlier save had right. */
    fun put(gx: Int, gy: Int, classes: ByteArray, right: Long) {
        if (right == 0L || classes.size != OUT * OUT || !ready) return
        synchronized(LOCKS[Math.floorMod(gx * 31 + gy, LOCKS.size)]) { merge(gx, gy, classes, right) }
    }

    /** put's read-merge-write, one writer per block at a time. */
    private fun merge(gx: Int, gy: Int, classes: ByteArray, right: Long) {
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

    /** Over [MAX_BLOCKS]: drop the least recently used (never one used this hour); older versions go entirely. */
    private fun prune(now: Long) {
        dir.parentFile?.listFiles()?.filter { it != dir }?.forEach { it.deleteRecursively() }
        val files = dir.listFiles() ?: return
        files.filter { it.name.endsWith(".part") && now - it.lastModified() > KEEP_RECENT_MS }.forEach { it.delete() }
        if (files.size <= MAX_BLOCKS) return
        files.sortedBy { it.lastModified() }.take(files.size - MAX_BLOCKS)
            .filter { now - it.lastModified() > KEEP_RECENT_MS }.forEach { it.delete() }
    }
}
