package dev.rotos.app.map

import dev.rotos.app.net.AppIdentity
import dev.rotos.app.net.Http
import dev.rotos.app.quota.GoogleApi
import dev.rotos.app.quota.UsageGuard
import dev.rotos.app.util.AppLog
import java.io.File

/**
 * Google map tiles on disk: <root>/<source dir>/z/x/y.png, root in filesDir
 * (Android may clear cacheDir; a cleared cache = every tile billed again).
 * Each tile is downloaded at most once: kept on disk, and two callers asking
 * for the same missing tile at once share one download. Never more than
 * [maxParallel] at a time; a download counts against the Google caps
 * (UsageGuard) only if it arrived. Blocking: call on Dispatchers.IO; thread-safe.
 */
class TileCache(private val root: File, private val source: Source) {
    /**
     * Where tiles come from: the URL for (z, x, y), the cache folder, parallel
     * downloads, and the billed API each download counts against (null = free).
     */
    class Source(
        val name: String, val dir: String, val maxParallel: Int,
        val headers: Map<String, String> = emptyMap(), val meter: GoogleApi? = null, val url: (Int, Int, Int) -> String,
    ) {
        companion object {
            fun google(tiles: GoogleTiles, session: String) = Source(
                "Google", "gmap/v${GoogleTiles.STYLE_VERSION}", 12, AppIdentity.googleHeaders(), GoogleApi.TILES,   // 12 at once: new areas fast
            ) { z, x, y -> tiles.tileUrl(z, x, y, session) }
        }
    }

    companion object {
        private const val TIMEOUT_MS = 30_000
        /** ~5-10 KB each (flat ROTOS style): a few hundred km of routes, ~150 MB at most. */
        const val MAX_TILES = 20_000
        private const val PRUNE_EVERY_MS = 3_600_000L
        private const val STALE_PART_MS = 600_000L
        /** A tile read is "used" again at most once a day (a write per read would be wasteful). */
        private const val TOUCH_MS = 86_400_000L
        private val LOCKS = Array(64) { Any() }
        private var prunedMs = 0L

        /**
         * Brings tiles saved under [from] (the old cacheDir home) into [to], file
         * by file (nothing re-downloaded, whatever happened before), drops older
         * styles' tiles, and at most hourly keeps [to] under [MAX_TILES], least
         * recently used first. Blocking (Dispatchers.IO).
         */
        @Synchronized
        fun settle(from: File, to: File, dir: String) {
            val old = File(from, dir)
            val new = File(to, dir)
            if (old.isDirectory) {
                new.parentFile?.mkdirs()
                if (!new.exists() && old.renameTo(new)) {
                    AppLog.i("tile cache: moved to ${new.path}")
                } else {
                    old.walkTopDown().filter { it.isFile && !it.name.endsWith(".part") }.forEach { f ->
                        val target = File(new, f.relativeTo(old).path)
                        if (!target.exists()) {
                            target.parentFile?.mkdirs()
                            if (!f.renameTo(target)) return@forEach
                        }
                    }
                    old.deleteRecursively()
                }
            }
            for (home in listOf(from, to)) {                       // older styles' tiles (not the session file)
                File(home, dir.substringBefore('/')).listFiles()
                    ?.filter { it.isDirectory && it.name != dir.substringAfter('/') }?.forEach { it.deleteRecursively() }
            }
            val now = System.currentTimeMillis()
            if (now - prunedMs < PRUNE_EVERY_MS) return
            prunedMs = now
            val files = new.walkTopDown().filter { it.isFile }.toList()
            files.filter { it.name.endsWith(".part") && now - it.lastModified() > STALE_PART_MS }   // cut short long ago,
                .forEach { it.delete() }                                                            // not one in flight
            if (files.size > MAX_TILES) files.sortedBy { it.lastModified() }.take(files.size - MAX_TILES).forEach { it.delete() }
        }
    }

    val maxParallel: Int get() = source.maxParallel
    val sourceName: String get() = source.name

    private val count = java.util.concurrent.atomic.AtomicInteger(0)

    /** Number of tiles actually downloaded (not served from disk). */
    val downloaded: Int get() = count.get()

    /** Tiles of today's cap this cache never takes (background warming leaves them to real maps). */
    @Volatile var keep: () -> Int = { 0 }

    fun cached(z: Int, x: Int, y: Int): Boolean = file(z, x, y).let { it.isFile && it.length() > 0 }

    fun file(z: Int, x: Int, y: Int): File = File(root, "${source.dir}/$z/$x/$y.png")

    /** PNG bytes of a tile on disk, or null: never downloads. Marks it used (pruning keeps used tiles). */
    fun read(z: Int, x: Int, y: Int): ByteArray? = file(z, x, y).takeIf { it.isFile && it.length() > 0 }
        ?.let { f ->
            val now = System.currentTimeMillis()
            if (now - f.lastModified() > TOUCH_MS) f.setLastModified(now)
            runCatching { f.readBytes() }.getOrNull()
        }

    /** Deletes a tile that turned out unusable (undecodable): the next build downloads it again. */
    fun forget(z: Int, x: Int, y: Int) {
        file(z, x, y).delete()
    }

    /** PNG bytes of a tile, from disk or the network (a billed download only if the caps allow it). */
    fun get(z: Int, x: Int, y: Int): Result<ByteArray> = synchronized(LOCKS[Math.floorMod(x * 31 + y, LOCKS.size)]) {
        read(z, x, y)?.let { return Result.success(it) }        // maybe a parallel caller just fetched it
        val f = file(z, x, y)
        var day = UsageGuard.today()
        source.meter?.let { api -> day = UsageGuard.acquire(api, keep = keep()).getOrElse { return Result.failure(it) } }
        val r = Http.get(source.url(z, x, y), TIMEOUT_MS, source.headers)
        r.onFailure { source.meter?.let { api -> UsageGuard.refund(api, 1, day) } }   // never arrived: not billed
        r.onSuccess { bytes ->
            count.incrementAndGet()
            val saved = runCatching {
                f.parentFile?.mkdirs()
                val tmp = File(f.path + "." + Thread.currentThread().id + ".part")
                tmp.writeBytes(bytes)
                tmp.renameTo(f).also { ok -> if (!ok) tmp.delete() }
            }.getOrDefault(false)
            if (!saved) AppLog.i("tile cache: could not save $z/$x/$y (it will be downloaded again)")
        }
        r
    }
}
