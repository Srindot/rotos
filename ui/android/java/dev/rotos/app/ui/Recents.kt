package dev.rotos.app.ui

import android.content.Context
import dev.rotos.app.route.LatLon
import dev.rotos.app.share.Destination
import org.json.JSONArray
import org.json.JSONObject

/** The last few destinations, newest first (SharedPreferences, JSON). */
class Recents(context: Context) {
    companion object {
        private const val PREFS = "rotos"
        private const val KEY = "recent_destinations"
        const val MAX = 5
    }

    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun list(): List<Destination> = runCatching {
        val a = JSONArray(prefs.getString(KEY, "[]"))
        (0 until a.length()).map { i ->
            val o = a.getJSONObject(i)
            Destination(o.getString("label"), LatLon(o.getDouble("lat"), o.getDouble("lon")))
        }
    }.getOrDefault(emptyList())

    fun add(d: Destination) {
        val keep = (listOf(d) + list().filter { it.label != d.label && it.at != d.at }).take(MAX)
        val a = JSONArray()
        keep.forEach { a.put(JSONObject().put("label", it.label).put("lat", it.at.lat).put("lon", it.at.lon)) }
        prefs.edit().putString(KEY, a.toString()).apply()
    }
}
