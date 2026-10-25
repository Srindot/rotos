package dev.rotos.app.mirror

/**
 * Google Maps arrow icons seen on real phones (ArrowClassifier.hash -> maneuver),
 * checked by eye from the saved icons/<hash>.png. They win over the
 * classifier; unknown icons fall back to it.
 */
object KnownArrows {
    private val TABLE: Map<Long, Int> = mapOf(
        0x000c0e2a08080800L to GuidanceParser.M_STRAIGHT,   // up arrow with dashed tail (Razr 60, Oct 2026)
    )

    fun lookup(hash: Long): Int? = TABLE[hash]
}
