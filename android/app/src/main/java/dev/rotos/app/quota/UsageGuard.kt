package dev.rotos.app.quota

import android.content.Context
import dev.rotos.app.util.AppLog
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.io.File
import java.time.LocalDate

/** One line of the usage card: an API, its counts and how close it is to the caps. */
data class Usage(val api: GoogleApi, val counts: Counts, val level: Level)

/**
 * The single gate in front of every billed Google call: GoogleRoutesClient,
 * GoogleTiles.session and Google tile downloads ask [acquire] first and fall
 * no new map or route when it says no (ROTOS keeps the Google Maps turns). Thread-safe.
 */
object UsageGuard {
    private const val FILE = "google_usage.json"

    private var ledger: UsageLedger? = null
    private val _usage = MutableStateFlow<List<Usage>>(emptyList())
    val usage: StateFlow<List<Usage>> = _usage

    @Synchronized
    fun init(context: Context) {
        if (ledger != null) return
        ledger = UsageLedger(File(context.filesDir, FILE))
        publish()
    }

    /** Today's date as the ledger counts it; give it back to [refund]. */
    fun today(): String = LocalDate.now().toString()

    /**
     * Counts [n] calls of [api] if the caps allow them, returning the day they
     * were counted against (for [refund]); failure (nothing counted) if not.
     * [keep]: calls of today's / this month's cap that must stay free for others
     * (background work leaves room for maps the rider is waiting for).
     */
    @Synchronized
    fun acquire(api: GoogleApi, n: Int = 1, purpose: Purpose = Purpose.USER, keep: Int = 0): Result<String> {
        val l = ledger ?: return Result.failure(IllegalStateException("usage guard not initialised"))
        val today = LocalDate.now().toString()
        val now = System.currentTimeMillis()
        val c = l.get(api, today)
        if (keep > 0 && minOf(api.dayCap - c.day, api.monthCap - c.month) - n < keep) {
            return Result.failure(QuotaException("${api.label}: leaving the last $keep for maps you ask for"))
        }
        return when (val v = UsagePolicy.decide(api, c, n, purpose, now)) {
            is Verdict.Denied -> {
                AppLog.i("Google call blocked: ${v.reason}")
                Result.failure(QuotaException(v.reason))
            }
            Verdict.Allowed -> {
                l.put(api, UsagePolicy.spend(c, n, now), today)
                publish()
                Result.success(today)                        // the day counted: give it to refund()
            }
        }
    }

    /**
     * Gives back [n] calls of [api] counted on [day] (from [today]) that never
     * reached Google (network error, refused): not billed, not counted. A call
     * from an earlier day only comes off the month it was counted in.
     */
    @Synchronized
    fun refund(api: GoogleApi, n: Int = 1, day: String = today()) {
        val l = ledger ?: return
        val today = LocalDate.now().toString()
        val c = l.get(api, today)
        val sameDay = day == today
        val sameMonth = day.take(7) == today.take(7)
        if (!sameMonth) return
        l.put(api, Counts(if (sameDay) (c.day - n).coerceAtLeast(0) else c.day, (c.month - n).coerceAtLeast(0), c.lastMs), today)
        publish()
    }

    /** Calls of [api] still allowed today and this month. */
    @Synchronized
    fun headroom(api: GoogleApi): Int {
        val c = ledger?.get(api, LocalDate.now().toString()) ?: return 0
        return minOf(api.dayCap - c.day, api.monthCap - c.month).coerceAtLeast(0)
    }

    /** True when any API is at or near its cap (ROTOS shows a warning). */
    fun nearCap(): Boolean = _usage.value.any { it.level != Level.OK }

    private fun publish() {
        val l = ledger ?: return
        val today = LocalDate.now().toString()
        _usage.value = GoogleApi.values().map { api ->
            val c = l.get(api, today)
            Usage(api, c, UsagePolicy.level(api, c))
        }
    }
}

class QuotaException(message: String) : Exception(message)
