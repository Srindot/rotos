package dev.rotos.app.route

/** What Google Maps is doing right now, from its navigation notification. */
data class GoogleTarget(
    val remainingM: Int?,
    val remainingS: Int?,
    /** Road names Google announced recently (newest first). */
    val roads: List<String>,
)

/**
 * Picks, among candidate routes, the one closest to what Google Maps is
 * navigating: distance and time left, and the road names it announces. Pure.
 */
object RouteMatcher {
    /** Index of the best candidate (0 when there is nothing to compare). */
    fun pick(routes: List<Route>, target: GoogleTarget?): Int {
        if (routes.size <= 1 || target == null) return 0
        return routes.indices.minByOrNull { score(routes[it], target) } ?: 0
    }

    /** Lower is better: relative distance + time error, and a penalty per announced road missing. */
    fun score(r: Route, t: GoogleTarget): Double {
        var s = 0.0
        t.remainingM?.takeIf { it > 0 }?.let { s += Math.abs(r.distanceM - it).toDouble() / it }
        t.remainingS?.takeIf { it > 0 }?.let { s += 0.5 * Math.abs(r.durationS - it) / it }
        val names = r.steps.map { RoadNames.ofInstruction(it.instruction) }.filter { it.isNotEmpty() }
        for (road in t.roads.take(3)) {
            if (names.none { RoadNames.similar(it, road) }) s += 0.35
        }
        return s
    }
}

/** Road-name comparison across Google and OpenStreetMap spellings. Pure. */
object RoadNames {
    private val ABBREV = mapOf(
        "rd" to "road", "st" to "street", "no" to "number", "nr" to "number", "ave" to "avenue",
        "hwy" to "highway", "ln" to "lane", "blvd" to "boulevard", "w" to "west", "e" to "east",
        "n" to "north", "s" to "south", "x" to "cross", "colony" to "colony", "nh" to "nh",
    )
    private val ROAD = Regex("""\b(?:onto|on|toward|towards)\s+(.+)$""", RegexOption.IGNORE_CASE)

    fun tokens(name: String): Set<String> = name.lowercase()
        .replace(Regex("""[^a-z0-9 ]"""), " ")
        .split(' ').filter { it.isNotBlank() }
        .map { ABBREV[it] ?: it }
        .toSet()

    private val GENERIC = setOf("road", "street", "number", "lane", "avenue", "highway", "cross", "main", "the", "of")

    /**
     * Same road: numbers identical (when both have them), and the distinctive
     * words ("jubilee", "gachibowli") mostly shared. Generic words alone
     * ("road", "street", "number") never make two names the same.
     */
    fun similar(a: String, b: String): Boolean {
        val ta = tokens(a)
        val tb = tokens(b)
        if (ta.isEmpty() || tb.isEmpty()) return false
        val na = ta.filter { t -> t.any { it.isDigit() } }.toSet()
        val nb = tb.filter { t -> t.any { it.isDigit() } }.toSet()
        if (na.isNotEmpty() && nb.isNotEmpty() && na != nb) return false
        val da = ta - GENERIC - na
        val db = tb - GENERIC - nb
        if (da.isEmpty() || db.isEmpty()) return da.isEmpty() && db.isEmpty() && na.isNotEmpty() && na == nb
        return da.intersect(db).size.toDouble() / minOf(da.size, db.size) >= 0.6
    }

    /** "Turn left onto W Jubilee Hills Rd" -> "W Jubilee Hills Rd". */
    /** The road in a step instruction's first line ("...Rd\nDestination will be on the right"). */
    fun ofInstruction(instruction: String): String =
        ROAD.find(instruction.lineSequence().firstOrNull().orEmpty())?.groupValues?.get(1)?.trim() ?: ""
}
