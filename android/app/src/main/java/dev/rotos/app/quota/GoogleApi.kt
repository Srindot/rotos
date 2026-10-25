package dev.rotos.app.quota

/**
 * Every billed Google Maps Platform call ROTOS makes, with its own caps.
 * Caps sit at about half of Google's monthly free tier, so a bug or a
 * runaway reroute loop runs into ROTOS's limit long before Google's bill.
 *
 *   ROUTES        Compute Routes (Pro: traffic-aware, two-wheeler)   5,000 free / month
 *   TILES         Map Tiles 2D, one per 256 px tile downloaded     100,000 free / month
 *   TILE_SESSION  Map Tiles createSession (cached ~2 weeks)         not billed, capped anyway
 *   PLACES        Places search (ids, free) + details (Essentials)  10,000 free / month
 */
enum class GoogleApi(
    val label: String,
    val monthCap: Int,
    val dayCap: Int,
    /** Shortest gap between two automatic calls (reroutes); 0 = none. */
    val autoGapMs: Long,
    /** Shortest gap between two calls the rider asked for (double share). */
    val userGapMs: Long,
) {
    ROUTES("Routes", 2_500, 200, 8_000L, 3_000L),
    TILES("Map tiles", 50_000, 8_000, 0L, 0L),
    TILE_SESSION("Tile sessions", 200, 20, 60_000L, 0L),
    PLACES("Place lookups", 2_000, 100, 0L, 0L),
}

/** Who asked: the rider (share, start ride) or ROTOS itself (reroute, map refresh). */
enum class Purpose { USER, AUTO }
