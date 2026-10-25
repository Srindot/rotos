package dev.rotos.app.map

import dev.rotos.app.nav.NativeNav
import java.io.File

/**
 * What a map transfer puts on the link (NavThread only):
 *   plain  the pack, raw deflate with a 4 KB window (~40-65 % smaller)
 *   patch  the pack as a patch against the pack ROTOS already holds
 *          (rn_delta.h), deflated: overlapping maps (reroutes, refreshed
 *          areas, rides from home) shrink to a few KB
 */
object LinkWire {
    /** Plain deflate of the current pack; bytes on the link ([len] when compression does not help). */
    fun plain(len: Int): Int {
        val pack = ByteArray(len)
        if (NativeNav.packCopy(pack) != len) return len
        val z = ByteArray(len + len / 1000 + 64)
        val n = NativeNav.deflateRaw(pack, len, z)
        return if (n in 1 until len && NativeNav.wireSet(z, n) == n) n else len
    }

    /** The smaller of plain and a patch against [base]; (bytes on the link, is a patch). */
    fun prepare(len: Int, base: PackBase.Copy?): Pair<Int, Boolean> {
        val plainLen = plain(len)
        if (base == null) return plainLen to false
        val patch = ByteArray(len + len / 8 + 1024)
        val p = NativeNav.delta(base.bytes, base.bytes.size, patch)
        if (p <= 0) return plainLen to false
        val z = ByteArray(p + p / 1000 + 64)
        val n = NativeNav.deflateRaw(patch, p, z)
        if (n <= 0 || n >= plainLen) {
            plain(len)                                        // keep the plain wire set
            return plainLen to false
        }
        return if (NativeNav.wireSetDelta(z, n, base.crc) == n) n to true else plain(len) to false
    }
}

/**
 * The pack ROTOS confirmed last (stored, or ACK_HAVE), kept on disk so a
 * restarted app can still patch against it. ROTOS checks the CRC, so a stale
 * copy only costs one full transfer.
 */
class PackBase(dir: File) {
    class Copy(val bytes: ByteArray, val crc: Int)

    private val file = File(dir, "rotos_base.rmap")
    private val crcFile = File(dir, "rotos_base.crc")
    @Volatile private var copy: Copy? = null

    fun get(): Copy? = copy ?: runCatching {
        Copy(file.readBytes(), crcFile.readText().trim().toLong(16).toInt())
    }.getOrNull()?.also { copy = it }

    /** ROTOS now holds the current pack (NavThread: reads it from the library). */
    fun remember(len: Int, crc: Int) {
        val bytes = ByteArray(len)
        if (NativeNav.packCopy(bytes) != len) return
        copy = Copy(bytes, crc)
        runCatching {
            file.writeBytes(bytes)
            crcFile.writeText("%08x".format(crc))
        }
    }
}
