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

    /** Counts [n] calls of [api] if the caps allow them; failure (nothing counted) if not. */
    @Synchronized
    fun acquire(api: GoogleApi, n: Int = 1, purpose: Purpose = Purpose.USER): Result<Unit> {
        val l = ledger ?: return Result.failure(IllegalStateException("usage guard not initialised"))
        val today = LocalDate.now().toString()
        val now = System.currentTimeMillis()
        val c = l.get(api, today)
        return when (val v = UsagePolicy.decide(api, c, n, purpose, now)) {
            is Verdict.Denied -> {
                AppLog.i("Google call blocked: ${v.reason}")
                Result.failure(QuotaException(v.reason))
            }
            Verdict.Allowed -> {
                l.put(api, UsagePolicy.spend(c, n, now), today)
                publish()
                Result.success(Unit)
            }
        }
    }

    /** Gives back [n] calls that never reached Google (network error, refused): not billed, not counted. */
    @Synchronized
    fun refund(api: GoogleApi, n: Int = 1) {
        val l = ledger ?: return
        val today = LocalDate.now().toString()
        val c = l.get(api, today)
        l.put(api, Counts((c.day - n).coerceAtLeast(0), (c.month - n).coerceAtLeast(0), c.lastMs), today)
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
