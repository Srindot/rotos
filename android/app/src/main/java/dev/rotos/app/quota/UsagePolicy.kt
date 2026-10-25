package dev.rotos.app.quota

/** Calls counted for one API: today, this month, and when the last one went out. */
data class Counts(val day: Int = 0, val month: Int = 0, val lastMs: Long = 0L)

enum class Level { OK, WARN, CAPPED }

sealed class Verdict {
    object Allowed : Verdict()
    data class Denied(val reason: String) : Verdict()
}

/** The caps as pure functions: no clock, no storage, no Android. */
object UsagePolicy {
    /** Share of a cap from which the phone and ROTOS warn. */
    const val WARN_SHARE = 0.8

    /** May [n] more calls of [api] go out at [nowMs]? */
    fun decide(api: GoogleApi, c: Counts, n: Int, purpose: Purpose, nowMs: Long): Verdict {
        require(n > 0) { "n must be positive" }
        val gap = if (purpose == Purpose.AUTO) api.autoGapMs else api.userGapMs
        return when {
            c.month + n > api.monthCap -> Verdict.Denied("${api.label}: monthly limit ${api.monthCap} reached")
            c.day + n > api.dayCap -> Verdict.Denied("${api.label}: daily limit ${api.dayCap} reached")
            gap > 0 && c.lastMs > 0 && nowMs - c.lastMs < gap ->
                Verdict.Denied("${api.label}: last call ${(nowMs - c.lastMs) / 1000} s ago, wait ${gap / 1000} s")
            else -> Verdict.Allowed
        }
    }

    /** How close [c] is to the caps of [api]. */
    fun level(api: GoogleApi, c: Counts): Level = when {
        c.month >= api.monthCap || c.day >= api.dayCap -> Level.CAPPED
        c.month >= api.monthCap * WARN_SHARE || c.day >= api.dayCap * WARN_SHARE -> Level.WARN
        else -> Level.OK
    }

    /** [c] after [n] calls at [nowMs]. */
    fun spend(c: Counts, n: Int, nowMs: Long): Counts = Counts(c.day + n, c.month + n, nowMs)

    /** [c] carried into ([day], [month]): a new day or month starts from zero. */
    fun roll(c: Counts, sameDay: Boolean, sameMonth: Boolean): Counts = Counts(
        day = if (sameDay) c.day else 0,
        month = if (sameMonth) c.month else 0,
        lastMs = c.lastMs,
    )
}
