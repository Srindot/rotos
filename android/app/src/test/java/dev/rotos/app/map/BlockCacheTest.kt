package dev.rotos.app.map

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.nio.file.Files

class BlockCacheTest {
    private val size = MapPackBuilder.OUT * MapPackBuilder.OUT

    @Test fun savedBlocksServeOnlyTheCellsTheyHaveRight() {
        val dir = Files.createTempDirectory("blocks").toFile()
        val classes = ByteArray(size) { (it % 7 + (it / 288) % 3).toByte() }
        val left = 0x0F0F0F0F0F0F0F0FL                                       // left half of the block right
        BlockCache(dir).put(1043, -2, classes, left)
        assertArrayEquals(classes, BlockCache(dir).get(1043, -2, TilePlan.bit(0, 0)))   // on disk: a new instance
        assertNull(BlockCache(dir).get(1043, -2, TilePlan.bit(7, 0)))                   // right half not built
        assertNull(BlockCache(dir).get(1044, -2, 1L))
        assertEquals(left, BlockCache(dir).mask(1043, -2))
        dir.deleteRecursively()
    }

    @Test fun laterBuildsAddCellsWithoutLosingEarlierOnes() {
        val dir = Files.createTempDirectory("blocks").toFile()
        val a = ByteArray(size) { 1 }
        val b = ByteArray(size) { 2 }
        val cache = BlockCache(dir)
        cache.put(5, 5, a, TilePlan.bit(0, 0))
        cache.put(5, 5, b, TilePlan.bit(7, 7))
        val both = TilePlan.bit(0, 0) or TilePlan.bit(7, 7)
        val got = cache.get(5, 5, both)!!
        val h = MapPackBuilder.HALO
        assertEquals(1.toByte(), got[h * MapPackBuilder.OUT + h])                            // cell (0,0) kept from a
        assertEquals(2.toByte(), got[(h + 7 * 32) * MapPackBuilder.OUT + h + 7 * 32])        // cell (7,7) from b
        dir.deleteRecursively()
    }
}
