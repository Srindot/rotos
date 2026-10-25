package dev.rotos.app.mirror

import java.time.LocalTime

/** One turn instruction from another navigation app, ready for a GUIDANCE packet. */
data class Guidance(
    val maneuver: Int,
    /** Metres to the turn, null when the notification does not say. */
    val toTurnM: Int?,
    val remainingM: Int?,
    /** Local arrival time, null when unknown. */
    val eta: LocalTime?,
    val road: String,
    val arrived: Boolean,
    /** Google Maps says "Rerouting...": it is choosing a new route. */
    val rerouting: Boolean = false,
)

/**
 * Google Maps' navigation notification text -> Guidance. Pure.
 *
 * The notification has no fixed public format, so this reads it the way a
 * person would: the first distance in the title (or before the instruction)
 * is the distance to the turn; the summary line ("12 min · 4.2 km · 17:58 ETA")
 * gives distance left and arrival; keywords in the instruction give the
 * maneuver ("turn right", "slight left", "roundabout", "U-turn", "keep left",
 * "merge", "exit"...); the road follows "onto" / "on" / "toward".
 * English wording; anything else still shows the distance with a straight arrow.
 */
object GuidanceParser {
    // RnManeuverType (navigation/include/rn/rn_maneuver.h)
    const val M_STRAIGHT = 2
    const val M_SLIGHT_LEFT = 3
    const val M_LEFT = 4
    const val M_SHARP_LEFT = 5
    const val M_UTURN_LEFT = 6
    const val M_SLIGHT_RIGHT = 7
    const val M_RIGHT = 8
    const val M_SHARP_RIGHT = 9
    const val M_UTURN_RIGHT = 10
    const val M_ROUNDABOUT_LEFT = 11
    const val M_ROUNDABOUT_RIGHT = 12
    const val M_FORK_LEFT = 13
    const val M_FORK_RIGHT = 14
    const val M_MERGE = 15
    const val M_RAMP_LEFT = 16
    const val M_RAMP_RIGHT = 17
    const val M_ARRIVE = 19

    private val DIST = Regex("""(\d+(?:[.,]\d+)?)\s*(km|m|mi|ft|yd)\b""", RegexOption.IGNORE_CASE)
    private val TIME = Regex("""\b(\d{1,2})[:.](\d{2})\s*([ap]\.?\s?m\.?)?""", RegexOption.IGNORE_CASE)
    private val MINUTES = Regex("""(?:(\d+)\s*(?:hr|h|hour)s?\s*)?(\d+)\s*min""", RegexOption.IGNORE_CASE)
    private val ROAD_KEYWORDS = listOf("onto", "towards", "toward", "on", "via", "to")
    private val ARRIVED = Regex("""\b(arrived|arrive at|your destination|destination is|you have reached)\b""",
        RegexOption.IGNORE_CASE)

    /**
     * [title], [text], [summary] are the notification's title, text and
     * sub/info text (any may be empty); [now] makes "12 min" into an arrival time.
     */
    fun parse(title: String, text: String, summary: String, now: LocalTime, iconManeuver: Int? = null): Guidance? {
        if (title.isBlank() && text.isBlank() && summary.isBlank()) return null
        val status = (title + " " + text).trim()
        if (STATUS.containsMatchIn(status)) {                 // "Rerouting...", "Starting navigation…"
            val m = DIST.findAll(summary).lastOrNull()?.let(::metres)
            return Guidance(M_STRAIGHT, null, m, eta(summary, now), "", false,
                rerouting = status.contains("rerout", ignoreCase = true))
        }
        val all = listOf(title, text, summary).filter { it.isNotBlank() }.joinToString(" · ") { it.trim() }
        val titleDist = DIST.find(title)
        val instruction = listOf(title, text).joinToString(" ") { stripLeadingDistance(it) }.trim()
        val toTurn = (titleDist ?: DIST.find(text)?.takeIf { it.range.first <= 8 })?.let(::metres)
        val remaining = DIST.findAll(summary).lastOrNull()?.let(::metres)
            ?: DIST.findAll(all).toList().let { d -> if (d.size >= 2) metres(d.last()) else null }
        val arrived = ARRIVED.containsMatchIn(all) && (toTurn == null || toTurn < 50)
        return Guidance(
            maneuver = when {
                arrived -> M_ARRIVE
                hasDirectionWords(instruction) -> maneuver(instruction)
                iconManeuver != null -> iconManeuver                 // text has no direction: trust the arrow
                else -> M_STRAIGHT
            },
            toTurnM = toTurn,
            remainingM = remaining,
            eta = eta(summary.ifBlank { all }, now),
            road = road(instruction),
            arrived = arrived,
        )
    }

    private val STATUS = Regex("""^(rerouting|starting navigation|getting route|gps signal|searching for gps)""", RegexOption.IGNORE_CASE)
    private val DIRECTION_WORDS = Regex("""\b(left|right|u-?turn|roundabout|rotary|keep|slight|sharp|merge|exit|ramp|fork)\b""",
        RegexOption.IGNORE_CASE)
    private val VERBS = Regex("""\b(turn|head|continue|make|take|keep|merge|arrive|starting)\b""", RegexOption.IGNORE_CASE)

    /** The instruction says which way (else the icon decides). */
    fun hasDirectionWords(instruction: String): Boolean = DIRECTION_WORDS.containsMatchIn(instruction)

    /** Pure: keyword -> RnManeuverType. */
    fun maneuver(instruction: String): Int {
        val s = instruction.lowercase()
        val left = Regex("""\bleft\b""").containsMatchIn(s)
        val right = Regex("""\bright\b""").containsMatchIn(s)
        return when {
            "u-turn" in s || "u turn" in s || "uturn" in s -> if (left) M_UTURN_LEFT else M_UTURN_RIGHT
            "roundabout" in s || "rotary" in s || "traffic circle" in s -> if (right) M_ROUNDABOUT_RIGHT else M_ROUNDABOUT_LEFT
            "slight" in s || "bear" in s -> if (left) M_SLIGHT_LEFT else M_SLIGHT_RIGHT
            "sharp" in s -> if (left) M_SHARP_LEFT else M_SHARP_RIGHT
            "keep" in s || "fork" in s -> if (left) M_FORK_LEFT else M_FORK_RIGHT
            "merge" in s -> M_MERGE
            "exit" in s || "ramp" in s || "take the" in s && ("flyover" in s || "elevated" in s) ->
                if (left) M_RAMP_LEFT else M_RAMP_RIGHT
            left -> M_LEFT
            right -> M_RIGHT
            else -> M_STRAIGHT
        }
    }

    /** The road after the strongest keyword: last "onto", else "toward(s)", "on", "via", "to". */
    fun road(instruction: String): String {
        val s = instruction.substringBefore(" · ")
        for (kw in ROAD_KEYWORDS) {
            val m = Regex("""\b$kw\s+""", RegexOption.IGNORE_CASE).findAll(s).lastOrNull() ?: continue
            return s.substring(m.range.last + 1).substringBefore(" then ").trim().trimEnd('.', ',')
        }
        // Motorola / Android 15 Google Maps: the text is just the road ("W Jubilee Hills Rd")
        val bare = s.trim()
        return if (bare.isNotEmpty() && !VERBS.containsMatchIn(bare) && !DIST.containsMatchIn(bare)) bare else ""
    }

    private fun eta(s: String, now: LocalTime): LocalTime? {
        TIME.find(s)?.let { m ->
            var h = m.groupValues[1].toInt()
            val min = m.groupValues[2].toInt()
            val ampm = m.groupValues[3].lowercase().replace(".", "").replace(" ", "")
            if (ampm == "pm" && h < 12) h += 12
            if (ampm == "am" && h == 12) h = 0
            if (h in 0..23 && min in 0..59) return LocalTime.of(h, min)
        }
        MINUTES.find(s)?.let { m ->
            val total = (m.groupValues[1].toIntOrNull() ?: 0) * 60 + m.groupValues[2].toInt()
            return now.plusMinutes(total.toLong())
        }
        return null
    }

    private fun metres(m: MatchResult): Int {
        val v = m.groupValues[1].replace(',', '.').toDouble()
        val f = when (m.groupValues[2].lowercase()) {
            "km" -> 1000.0
            "mi" -> 1609.344
            "ft" -> 0.3048
            "yd" -> 0.9144
            else -> 1.0
        }
        return (v * f).toInt()
    }

    private fun stripLeadingDistance(s: String): String =
        s.trim().replace(Regex("""^(in\s+)?\d+(?:[.,]\d+)?\s*(km|m|mi|ft|yd)\b[,:]?\s*""", RegexOption.IGNORE_CASE), "")
}
