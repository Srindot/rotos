package dev.rotos.app.quota

import org.json.JSONObject
import java.io.File

/**
 * The counters on disk (filesDir/google_usage.json), so caps hold across app
 * restarts and reinstalls of the same build. Days and months are local
 * dates ("2026-10-24", "2026-10"); a new day or month resets its counter.
 * Not thread-safe: UsageGuard serialises access.
 */
class UsageLedger(private val file: File) {
    private var day = ""
    private var month = ""
    private val counts = HashMap<GoogleApi, Counts>()

    init {
        load()
    }

    /** Counts of [api] as of [today] ("yyyy-MM-dd"). */
    fun get(api: GoogleApi, today: String): Counts {
        rollTo(today)
        return counts[api] ?: Counts()
    }

    fun put(api: GoogleApi, c: Counts, today: String) {
        rollTo(today)
        counts[api] = c
        save()
    }

    private fun rollTo(today: String) {
        if (today == day) return
        val thisMonth = today.take(7)
        for (api in GoogleApi.values()) {
            counts[api] = UsagePolicy.roll(counts[api] ?: Counts(), sameDay = false, sameMonth = thisMonth == month)
        }
        day = today
        month = thisMonth
        save()
    }

    private fun load() = runCatching {
        val o = JSONObject(file.readText())
        day = o.optString("day")
        month = o.optString("month")
        val c = o.getJSONObject("counts")
        for (api in GoogleApi.values()) {
            val a = c.optJSONObject(api.name) ?: continue
            counts[api] = Counts(a.optInt("day"), a.optInt("month"), a.optLong("last"))
        }
    }

    private fun save() = runCatching {
        val c = JSONObject()
        counts.forEach { (api, n) -> c.put(api.name, JSONObject().put("day", n.day).put("month", n.month).put("last", n.lastMs)) }
        val json = JSONObject().put("day", day).put("month", month).put("counts", c).toString()
        file.parentFile?.mkdirs()
        val tmp = File(file.path + ".tmp")
        tmp.writeText(json)
        tmp.renameTo(file)
    }
}
