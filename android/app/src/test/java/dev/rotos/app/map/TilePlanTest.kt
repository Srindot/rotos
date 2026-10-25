package dev.rotos.app.map

import dev.rotos.app.route.LatLon
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class TilePlanTest {
    @Test fun worldPixelsMatchWebMercator() {
        val (x, y) = MapPackBuilder.worldPx(LatLon(0.0, 0.0))
        assertEquals(256.0 * (1 shl 16) / 2, x, 1e-6)
        assertEquals(256.0 * (1 shl 16) / 2, y, 1e-6)
    }

    @Test fun aCellNeedsOnlyTheTilesUnderItAndItsHalo() {
        // cell (8, 8): z16 px 256..288, halo 240..304 -> z17 480..608 -> tiles 1..2 in each direction
        assertEquals(setOf(1 to 1, 2 to 1, 1 to 2, 2 to 2), TilePlan.tilesOfCell(8, 8).toSet())
        // a cell in the middle of a z17 tile: just that tile
        assertEquals(listOf(5 to 5), TilePlan.tilesOfCell(8 * 5 / 2 + 1, 8 * 5 / 2 + 1))
    }

    @Test fun aCornerOfABlockCostsFarFewerTilesThanTheWholeBlock() {
        val corner = TilePlan.byBlock(listOf(16 to 16))                     // one cell of block (2, 2)
        val whole = TilePlan.byBlock((16 until 24).flatMap { x -> (16 until 24).map { y -> x to y } })
        assertEquals(mapOf((2 to 2) to 1L), corner)
        assertEquals(mapOf((2 to 2) to -1L), whole)                          // all 64 bits
        assertTrue(TilePlan.tilesFor(corner).size <= 4)
        assertEquals(16, TilePlan.tilesFor(whole).size)
    }

    @Test fun rightCellsAreThoseWithAllTheirTiles() {
        val all = TilePlan.rightCells(2, 2) { _, _ -> true }
        assertEquals(-1L, all)
        val none = TilePlan.rightCells(2, 2) { _, _ -> false }
        assertEquals(0L, none)
        val have = TilePlan.tilesFor(TilePlan.byBlock(listOf(16 to 16))).toSet()
        val m = TilePlan.rightCells(2, 2) { x, y -> (x to y) in have }
        assertTrue(m and TilePlan.bit(0, 0) != 0L)                         // the planned cell is right
        assertTrue(m and TilePlan.bit(7, 7) == 0L)                         // a far corner is not
    }

    @Test fun cellsAroundFollowTheLine() {
        val a = LatLon(17.44, 78.35)
        val b = LatLon(17.44, 78.40)                                        // ~5.3 km east
        val near = TilePlan.cellsAround(listOf(a), 0)
        assertEquals(1, near.size)
        val (ax, ay) = MapPackBuilder.worldPx(a)
        assertEquals(Math.floorDiv(ax.toInt(), 32) to Math.floorDiv(ay.toInt(), 32), near.first())
        val band = TilePlan.cellsAround(listOf(a, b), 300).toList()
        assertTrue(band.first().first < band.last().first)                  // start of the line first
        assertTrue(band.all { (_, cy) -> Math.abs(cy * 32 + 16 - ay) < 300 + 32 })
    }
}
