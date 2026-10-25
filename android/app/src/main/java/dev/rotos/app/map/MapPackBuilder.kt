package dev.rotos.app.map

import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.Packets
import dev.rotos.app.nav.NavThread
import dev.rotos.app.quota.GoogleApi
import dev.rotos.app.quota.Purpose
import dev.rotos.app.quota.UsageGuard
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.LoadedRoute
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/** Result of a map pack build (the pack itself stays inside the C library). */
data class PackInfo(
    val length: Int,
    val crc32: Int,
    val routeId: Int,
    val originX: Int,
    val originY: Int,
    val tilesW: Int,
    val tilesH: Int,
    val keptCells: Int,
    val blocks: Int,
    val tilesDownloaded: Int,
    val tilesMissing: Int,
    val flyover: String,
    /** Where the map came from ("Google"). */
    val tileSource: String = "Google",
    /** Bytes the transfer sends: the pack compressed for the link, or [length] if raw. */
    val wireLength: Int = length,
    /** Blocks taken from BlockCache (no tiles read, no classifier). */
    val blocksSaved: Int = 0,
)

/**
 * Planning progress. [prep] is the PREP_STATUS stage sent to ROTOS
 * (Packets.PREP_*), [pct] the whole job 0..100, [done]/[total] in stage units
 * (map blocks; 0/0 when the stage has no count), [tiles] map tiles downloaded.
 */
data class BuildProgress(
    val stage: String,
    val done: Int,
    val total: Int,
    val prep: Int = Packets.PREP_ROUTE,
    val pct: Int = 0,
    val tiles: Int = 0,
)

/**
 * The map pack recipe of docs/PROTOCOL.md ("Phone side map pack recipe"),
 * steps 2-5, for the route already loaded in NativeNav:
 *   flyover hints from road names -> corridor_plan(16, 260)
 *   -> per 8x8-cell block holding a kept cell: the saved block (BlockCache)
 *      if its right cells cover the kept ones, else a 576x576 z17 RGB window
 *      with a 16 px (z16) halo -> mapgen_block -> each kept cell's 32x32 from
 *      the interior at (16, 16) -> pack_add_tile -> pack_finish.
 * Tiles: only those under kept cells (TilePlan) and not on disk are
 * downloaded - a route seen before costs no tiles at all.
 * Network on Dispatchers.IO, library calls on NavThread.
 */
class MapPackBuilder(
    private val cacheDir: File,
    private val googleKey: String = "",
    /** Where converted blocks are kept between maps (BlockCache); null = none. */
    private val blocks: BlockCache? = null,
    /** Where Google tiles are kept (filesDir: Android never clears it); tiles found in cacheDir move here. */
    private val tilesDir: File = cacheDir,
) {
    init {
        if (tilesDir != cacheDir) TileCache.settle(cacheDir, tilesDir, "gmap/v${GoogleTiles.STYLE_VERSION}")
    }

    companion object {
        const val ZOOM = 16
        const val BUFFER_PX = 260            // area maps: rows 0.8 km apart must overlap
        /** Route rides: ~410 m either side of the line (smaller pack, faster upload). */
        const val ROUTE_BUFFER_PX = 180
        const val CELL = 32
        const val BLOCK_CELLS = 8
        const val BLOCK_PX = CELL * BLOCK_CELLS          // 256 px at zoom 16
        const val HALO = 16                               // px at zoom 16
        const val WIN = (BLOCK_PX + 2 * HALO) * 2         // 576 px at zoom 17
        const val OUT = WIN / 2                           // 288 classes
        private const val MAX_BLOCKS = 1500               // ~100 km of route: refuse bulk downloads
        /** Background warming leaves this many tiles of today's cap for real maps. */
        private const val WARM_RESERVE_TILES = 2_000

        /** Web Mercator world pixels at [ZOOM] (as rn_geo_world_px). Pure. */
        fun worldPx(p: LatLon): Pair<Double, Double> {
            val n = 256.0 * (1 shl ZOOM)
            val lat = Math.toRadians(p.lat.coerceIn(-85.0511, 85.0511))
            val x = (p.lon + 180.0) / 360.0 * n
            val y = (1.0 - Math.log(Math.tan(lat) + 1.0 / Math.cos(lat)) / Math.PI) / 2.0 * n
            return x to y
        }
    }

    /** What [runBlocks] did: tiles downloaded, needed tiles still missing, blocks from BlockCache / built. */
    private class BlockRun(val downloaded: Int, val missing: Int, val fromCache: Int, val built: Int)

    /**
     * In the background, makes sure the map within [radiusPx] of [points] is
     * saved (BlockCache), so a reroute or detour later finds it ready - built
     * in well under a second, no downloads mid-ride. Frugal like any build:
     * only the tiles under those cells, only once; it leaves
     * [WARM_RESERVE_TILES] of today's cap for real maps. One block per
     * NavThread call, so real map builds are never held up.
     * Returns (blocks built, tiles downloaded).
     */
    suspend fun warm(points: List<LatLon>, radiusPx: Int): Pair<Int, Int> {
        if (blocks == null) return 0 to 0
        val need = TilePlan.byBlock(TilePlan.cellsAround(points, radiusPx))
        val run = runBlocks(need, Purpose.AUTO, WARM_RESERVE_TILES, loadSaved = false, progress = null) { _, _ -> true }
        return run.fold({ it.built to it.downloaded }, { 0 to 0 })
    }

    /**
     * [flyovers]: flyover hints from road names for ROTOS's own turns. Off when
     * Google Maps navigates (Google gives the turns).
     * [purpose]: who asked, for the Google usage caps (UsageGuard).
     */
    suspend fun build(
        route: LoadedRoute, progress: (BuildProgress) -> Unit, flyovers: Boolean = true, bufferPx: Int = BUFFER_PX,
        purpose: Purpose = Purpose.USER,
    ): Result<PackInfo> {
        val flyover = if (flyovers) {
            progress(BuildProgress("checking flyovers", 0, 0, Packets.PREP_BRIDGES, 10))
            flyoverHints()
        } else {
            "not needed (Google Maps navigates)"
        }
        // corridor and kept cells in ONE NavThread call: nothing else (BlockCache warming) can slip in between
        val planned = NavThread.run {
            planCorridor(bufferPx).map { info ->
                val bw = (info[2] + BLOCK_CELLS - 1) / BLOCK_CELLS
                val bh = (info[3] + BLOCK_CELLS - 1) / BLOCK_CELLS
                val cells = LinkedHashMap<Pair<Int, Int>, List<Pair<Int, Int>>>()
                if (bw * bh <= MAX_BLOCKS) {
                    for (by in 0 until bh) for (bx in 0 until bw) keptCells(bx, by).takeIf { it.isNotEmpty() }?.let { cells[bx to by] = it }
                }
                Triple(info, bw * bh, cells)
            }
        }.getOrElse { return Result.failure(it) }
        val (corridor, nBlocks, cellsOf) = planned
        val (ox, oy, tw, th, nKeep) = corridor
        if (nBlocks > MAX_BLOCKS) return Result.failure(IllegalStateException("corridor too large: $nBlocks blocks"))
        if (Math.floorMod(ox, BLOCK_PX) != 0 || Math.floorMod(oy, BLOCK_PX) != 0) {
            return Result.failure(IllegalStateException("corridor origin $ox,$oy off the block grid"))
        }
        val gx0 = ox / BLOCK_PX
        val gy0 = oy / BLOCK_PX
        val need = LinkedHashMap<Pair<Int, Int>, Long>()            // global block -> kept cells, pack order
        for ((b, cells) in cellsOf) {
            var m = 0L
            for ((cx, cy) in cells) m = m or TilePlan.bit(cx - b.first * BLOCK_CELLS, cy - b.second * BLOCK_CELLS)
            need[(gx0 + b.first) to (gy0 + b.second)] = m
        }
        var done = 0
        val run = runBlocks(need, purpose, 0, loadSaved = true, progress = progress) { g, classes ->
            val b = (g.first - gx0) to (g.second - gy0)
            val st = NavThread.run { addCells(classes!!, b.first, b.second, cellsOf.getValue(b)) }
            done++
            progress(mapProgress(done, need.size))
            st >= 0
        }.getOrElse { return Result.failure(it) }
        val len = NavThread.run { NativeNav.packFinish() }
        if (len < 0) return Result.failure(IllegalStateException("pack_finish failed (rn_status $len)"))
        val (crc, id) = NavThread.run { NativeNav.packCrc32() to NativeNav.packRouteId() }
        val wire = NavThread.run { LinkWire.plain(len) }
        progress(BuildProgress("map ready", 0, 0, Packets.PREP_READY, 100, run.downloaded))
        return Result.success(
            PackInfo(len, crc, id, ox, oy, tw, th, nKeep, done, run.downloaded,
                run.missing, flyover, "Google", wire, run.fromCache),
        )
    }

    /**
     * The frugal core (TilePlan). For each global block of [need] (block ->
     * cells it must get right), in order: the saved block if its right cells
     * cover them, else it is converted from a tile window - after downloading
     * only the tiles under the needed cells that are not on disk yet, and
     * saved with its right cells. [use] gets every block's classes (null for
     * saved ones when ![loadSaved]: warming skips them) and returns false to
     * stop. [reserve]: tiles of today's cap left alone (warming); a build
     * fails instead if its tiles do not fit the cap.
     */
    private suspend fun runBlocks(
        need: Map<Pair<Int, Int>, Long>, purpose: Purpose, reserve: Int, loadSaved: Boolean,
        progress: ((BuildProgress) -> Unit)?, use: suspend (Pair<Int, Int>, ByteArray?) -> Boolean,
    ): Result<BlockRun> = coroutineScope {
        val saved = HashMap<Pair<Int, Int>, ByteArray>()
        val todo = LinkedHashMap<Pair<Int, Int>, Long>()
        withContext(Dispatchers.IO) {
            for ((g, m) in need) {
                val hit = when {
                    blocks == null -> false
                    loadSaved -> blocks.get(g.first, g.second, m)?.also { saved[g] = it } != null
                    else -> blocks.mask(g.first, g.second) and m == m
                }
                if (!hit) todo[g] = m
            }
        }
        val g = GoogleTiles(googleKey, cacheDir)
        var tiles = TileCache(tilesDir, TileCache.Source.google(g, ""))          // disk only until a download is due
        val wanted = TilePlan.tilesFor(todo)
        var missing = withContext(Dispatchers.IO) { wanted.filter { (x, y) -> !tiles.cached(ZOOM + 1, x, y) } }
        if (missing.isNotEmpty()) {
            if (googleKey.isBlank()) return@coroutineScope Result.failure(IllegalStateException("no Google API key"))
            val left = UsageGuard.headroom(GoogleApi.TILES) - reserve
            if (missing.size > left) {
                if (reserve == 0) {
                    return@coroutineScope Result.failure(IllegalStateException("Google tile limit: ${missing.size} needed, $left left"))
                }
                missing = missing.take(left.coerceAtLeast(0))               // warming: as far as the cap allows
            }
        }
        if (missing.isNotEmpty()) {
            val session = withContext(Dispatchers.IO) { g.session(purpose) }.getOrElse {
                return@coroutineScope Result.failure(IllegalStateException("Google map unavailable: ${it.message?.take(120)}"))
            }
            tiles = TileCache(tilesDir, TileCache.Source.google(g, session))
            download(tiles, missing, progress)
        }
        val window = TileWindow(tiles, ZOOM + 1)
        NavThread.run { NativeNav.mapgenSource(1) }                  // Google style: keep every narrow lane
        progress?.invoke(mapProgress(0, need.size))
        for ((blk, m) in need) {
            val classes = saved[blk] ?: if (blk !in todo) null else {
                val x0 = (blk.first * BLOCK_PX - HALO) * 2
                val y0 = (blk.second * BLOCK_PX - HALO) * 2
                val failed = window.failed
                val rgb = withContext(Dispatchers.IO) { window.rgb(x0, y0, WIN, WIN) }
                val out = ByteArray(OUT * OUT)
                val st = NavThread.run {
                    NativeNav.mapgenSource(1)
                    NativeNav.mapgenBlock(rgb, WIN, WIN, out)
                }
                if (st < 0) return@coroutineScope Result.failure(IllegalStateException("block $blk failed (rn_status $st)"))
                if (window.failed == failed) withContext(Dispatchers.IO) {
                    blocks?.put(blk.first, blk.second, out,
                        TilePlan.rightCells(blk.first, blk.second) { tx, ty -> tiles.cached(ZOOM + 1, tx, ty) })
                }
                out
            }
            if (classes == null && loadSaved) continue
            if (!use(blk, classes)) break
        }
        val stillMissing = withContext(Dispatchers.IO) { wanted.count { (x, y) -> !tiles.cached(ZOOM + 1, x, y) } }
        Result.success(BlockRun(tiles.downloaded, stillMissing, saved.size, todo.size))
    }

    /** Building blocks takes the job from 75 % to 99 %. */
    private fun mapProgress(done: Int, total: Int) = BuildProgress(
        "building map", done, total, Packets.PREP_MAP, 75 + if (total > 0) 24 * done / total else 0, 0,
    )

    /** Downloads [missing] tiles, TileCache.maxParallel at a time (15 % .. 75 %). */
    private suspend fun download(
        tiles: TileCache, missing: List<Pair<Int, Int>>, progress: ((BuildProgress) -> Unit)?,
    ) = withContext(Dispatchers.IO) {
        val z = ZOOM + 1
        val done = java.util.concurrent.atomic.AtomicInteger(0)
        val queue = java.util.concurrent.ConcurrentLinkedQueue(missing)
        coroutineScope {
            repeat(tiles.maxParallel) {
                launch {
                    while (true) {
                        val (x, y) = queue.poll() ?: break
                        tiles.get(z, x, y)
                        val n = done.incrementAndGet()
                        progress?.invoke(BuildProgress("downloading map tiles", n, missing.size, Packets.PREP_MAP,
                            15 + 60 * n / missing.size, n))
                    }
                }
            }
        }
    }

    /** Flyover hints from road names (no map data needed). NavThread. */
    private suspend fun flyoverHints(): String = NavThread.run {
        NativeNav.flyoverByName()
        "road names"
    }

    /** [origin_x, origin_y, tiles_w, tiles_h, n_keep] after planning and pack_begin. */
    private fun planCorridor(bufferPx: Int): Result<IntArray> {
        val n = NativeNav.corridorPlan(ZOOM, bufferPx)
        if (n < 0) return Result.failure(IllegalStateException("corridor_plan failed (rn_status $n)"))
        val info = IntArray(5)
        val st = NativeNav.corridorInfo(info)
        if (st < 0) return Result.failure(IllegalStateException("corridor_info failed (rn_status $st)"))
        val b = NativeNav.packBegin()
        if (b < 0) return Result.failure(IllegalStateException("pack_begin failed (rn_status $b)"))
        return Result.success(info)
    }

    private fun keptCells(bx: Int, by: Int): List<Pair<Int, Int>> {
        val out = ArrayList<Pair<Int, Int>>()
        for (ly in 0 until BLOCK_CELLS) {
            for (lx in 0 until BLOCK_CELLS) {
                val cx = bx * BLOCK_CELLS + lx
                val cy = by * BLOCK_CELLS + ly
                if (NativeNav.corridorKeep(cx, cy) == 1) out.add(cx to cy)
            }
        }
        return out
    }

    /** The kept cells of block (bx, by) from its [classes] into the pack. NavThread. */
    private fun addCells(classes: ByteArray, bx: Int, by: Int, cells: List<Pair<Int, Int>>): Int {
        val tile = ByteArray(CELL * CELL)
        for ((cx, cy) in cells) {
            val ix = HALO + (cx - bx * BLOCK_CELLS) * CELL
            val iy = HALO + (cy - by * BLOCK_CELLS) * CELL
            for (row in 0 until CELL) {
                System.arraycopy(classes, (iy + row) * OUT + ix, tile, row * CELL, CELL)
            }
            val a = NativeNav.packAddTile(cx, cy, tile)
            if (a < 0) return a
        }
        return 0
    }
}
