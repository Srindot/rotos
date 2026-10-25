package dev.rotos.app.ride

import android.content.Context
import dev.rotos.app.route.LatLon
import dev.rotos.app.share.DestinationResolver

/** A destination the rider shared, and when. */
data class SharedRoute(val destination: LatLon, val savedMs: Long)

/**
 * Whether a remembered shared route should come back after the app was
 * restarted (update, Android killing it) while Google Maps still navigates.
 * Pure: the caller gives the clock and the places.
 */
object ResumeRule {
    const val MAX_AGE_MS = 3 * 3600_000L
    private const val ARRIVED_M = 150.0
    private const val SLACK_M = 500.0

    fun resume(saved: SharedRoute?, nowMs: Long, here: LatLon, googleRemainingM: Int?): LatLon? {
        if (saved == null || nowMs - saved.savedMs !in 0..MAX_AGE_MS) return null
        val straight = DestinationResolver.distanceM(here, saved.destination)
        if (straight < ARRIVED_M) return null
        val g = googleRemainingM?.toDouble() ?: return saved.destination
        // a road is never shorter than the straight line, nor (here) more than ~3x longer
        return if (straight <= g + SLACK_M && g <= 3 * straight + 5_000) saved.destination else null
    }
}

/** The last shared route, kept across app restarts (SharedPreferences). */
class RouteMemory(context: Context) {
    private val prefs = context.getSharedPreferences("shared_route", Context.MODE_PRIVATE)

    fun save(destination: LatLon) {
        prefs.edit().putString("lat", destination.lat.toString()).putString("lon", destination.lon.toString())
            .putLong("at", System.currentTimeMillis()).apply()
    }

    fun load(): SharedRoute? {
        val lat = prefs.getString("lat", null)?.toDoubleOrNull() ?: return null
        val lon = prefs.getString("lon", null)?.toDoubleOrNull() ?: return null
        return SharedRoute(LatLon(lat, lon), prefs.getLong("at", 0L))
    }

    fun forget() {
        prefs.edit().remove("lat").remove("lon").remove("at").apply()
    }

    /** The rider stopped while Google Maps navigates: survives app restarts until that navigation ends. */
    var stopped: Boolean
        get() = System.currentTimeMillis() - prefs.getLong("stopped_at", 0L) in 0..ResumeRule.MAX_AGE_MS
        set(on) {
            prefs.edit().putLong("stopped_at", if (on) System.currentTimeMillis() else 0L).apply()
        }
}

/**
 * The ROTOS session the rider switched on, kept until they switch it off, so
 * the app reconnects by itself after Android killed it or an update replaced
 * it (mid-ride ROTOS would otherwise say NO PHONE LINK until the app is opened).
 */
class SessionMemory(context: Context) {
    private val prefs = context.getSharedPreferences("session", Context.MODE_PRIVATE)

    fun save(cfg: RideConfig) {
        if (cfg.demo || cfg.useSample) return                   // test rides are not brought back
        prefs.edit().putBoolean("on", true).putString("link", cfg.link.name).putString("host", cfg.host)
            .putInt("port", cfg.port).putBoolean("calls", cfg.callAlerts).putBoolean("notify", cfg.notifyAlerts)
            .apply()
    }

    fun clear() {
        prefs.edit().putBoolean("on", false).apply()
    }

    /** The session to restore (a placeholder origin: rides start from GPS), or null if switched off. */
    fun load(): RideConfig? {
        if (!prefs.getBoolean("on", false)) return null
        val link = runCatching { LinkKind.valueOf(prefs.getString("link", "BLE") ?: "BLE") }.getOrDefault(LinkKind.BLE)
        val nowhere = LatLon(0.0, 0.0)
        return RideConfig(origin = nowhere, destination = nowhere, useSample = false, demo = false, speedup = 1f,
            link = link, host = prefs.getString("host", "10.0.2.2") ?: "10.0.2.2", port = prefs.getInt("port", 7400),
            callAlerts = prefs.getBoolean("calls", true), notifyAlerts = prefs.getBoolean("notify", true))
    }
}
