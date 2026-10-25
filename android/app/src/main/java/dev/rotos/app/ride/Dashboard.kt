package dev.rotos.app.ride

import android.content.Context
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import dev.rotos.app.nav.Packets
import dev.rotos.app.phone.PhoneStatusReader
import dev.rotos.app.phone.WeatherClient
import dev.rotos.app.quota.UsageGuard
import dev.rotos.app.route.LatLon
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * ROTOS's dashboard feed while the phone is connected, ride or not:
 * PHONE_STATUS every 5 s, WEATHER + FORECAST every 10 min (1 min after a
 * failure), PING every 2 s when nothing else went out.
 */
class Dashboard(
    private val context: Context,
    private val send: FrameSender,
    /** Where the rider is now, or null. */
    private val position: () -> LatLon?,
    private val log: (String) -> Unit,
) {
    companion object {
        private const val STATUS_MS = 5_000L
        private const val WEATHER_MS = 10 * 60_000L
        private const val WEATHER_RETRY_MS = 60_000L
        private const val PING_MS = 2_000L
        private const val TICK_MS = 500L
        /** Sun this far below the horizon (degrees, refraction included) = night. */
        const val NIGHT_SUN_DEG = -0.833

        fun isNight(lat: Double, lon: Double, unixS: Double): Boolean =
            NativeNav.sunElevation(lat, lon, unixS) < NIGHT_SUN_DEG
    }

    /** Runs until cancelled (the session ends). [fallback]: weather spot without a position. */
    suspend fun run(fallback: LatLon) = coroutineScope {
        val reader = PhoneStatusReader(context)
        var nextStatus = 0L
        var nextWeather = 0L
        while (isActive) {
            val now = System.currentTimeMillis()
            if (now >= nextStatus) {
                phoneStatus(reader)
                nextStatus = now + STATUS_MS
            }
            if (now >= nextWeather) {
                nextWeather = now + WEATHER_MS
                val at = position() ?: fallback
                launch { if (!weather(at)) nextWeather = System.currentTimeMillis() + WEATHER_RETRY_MS }
            }
            if (now - send.lastSentMs >= PING_MS) send.ping()
            delay(TICK_MS)
        }
    }

    private suspend fun phoneStatus(reader: PhoneStatusReader) {
        val s = reader.read()
        val at = position()
        val unixS = System.currentTimeMillis() / 1000.0
        val night = at != null && NavThread.run { isNight(at.lat, at.lon, unixS) }
        val flags = (if (s.charging) Packets.STATUS_CHARGING else 0) or
            (if (night) Packets.STATUS_NIGHT else 0) or
            (if (UsageGuard.nearCap()) Packets.STATUS_GOOGLE_CAP else 0)
        send.phoneStatus(s, flags)
    }

    /** WEATHER then FORECAST; false when Open-Meteo could not be reached (retry sooner). */
    private suspend fun weather(at: LatLon): Boolean {
        val w = withContext(Dispatchers.IO) { WeatherClient().conditions(at.lat, at.lon) }
        w.onSuccess { c ->
            val x = c.now
            log("weather %.1f C code %d rain %d%% wind %d km/h".format(x.tempC, x.code, x.rainPct, x.windKmh))
            send.weather(x)
            val f = c.forecast
            if (f == null) {
                log("forecast unavailable in the weather answer")
            } else {
                log("forecast from %02d:00, %d h, sunrise %02d:%02d sunset %02d:%02d".format(f.startHour,
                    minOf(f.hours.size, WeatherClient.HOURS), f.sunriseMin / 60, f.sunriseMin % 60,
                    f.sunsetMin / 60, f.sunsetMin % 60))
                send.forecast(f)
            }
        }.onFailure { log("weather unavailable (retry in 60 s): ${it.message}") }
        return w.isSuccess
    }
}
