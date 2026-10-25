package dev.rotos.app.phone

import dev.rotos.app.net.Http
import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

/**
 * Current conditions + the next 8 hours + today's sunrise/sunset from
 * Open-Meteo (free, no key), all in local time (timezone=auto).
 * Blocking: call on IO.
 */
class WeatherClient {
    companion object {
        const val HOURS = 8
        private const val URL = "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f" +
            "&current=temperature_2m,weather_code,precipitation_probability,wind_speed_10m,is_day" +
            "&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m" +
            "&forecast_hours=10&daily=sunrise,sunset&forecast_days=1&timezone=auto"
        private const val TIMEOUT_MS = 15_000

        /** "2026-10-10T06:08" -> 368 (local minutes of day); null when malformed. */
        fun minuteOfDay(iso: String): Int? {
            val t = iso.substringAfter('T', "")
            val h = t.substringBefore(':').toIntOrNull() ?: return null
            val m = t.substringAfter(':', "").take(2).toIntOrNull() ?: return null
            return if (h in 0..23 && m in 0..59) h * 60 + m else null
        }

        /** Pure: Open-Meteo JSON -> conditions. The forecast is null when its blocks are missing. */
        fun parse(json: String): Conditions {
            val root = JSONObject(json)
            val c = root.getJSONObject("current")
            val hourly = root.optJSONObject("hourly")
            val rain = when {
                c.has("precipitation_probability") && !c.isNull("precipitation_probability") ->
                    c.getInt("precipitation_probability")
                else -> hourly?.optJSONArray("precipitation_probability")?.optInt(0, 0) ?: 0
            }
            val now = Weather(
                tempC = c.getDouble("temperature_2m").toFloat(),
                code = c.getInt("weather_code"),
                rainPct = rain.coerceIn(0, 100),
                windKmh = Math.round(c.getDouble("wind_speed_10m")).toInt().coerceIn(0, 255),
                isDay = c.optInt("is_day", 1) == 1,
            )
            val hourNow = c.optString("time", "").take(13)            // "2026-10-10T17"
            return Conditions(now, runCatching { forecast(hourly, root.optJSONObject("daily"), hourNow) }.getOrNull())
        }

        /** The 8 hours starting at the current local hour (Open-Meteo also returns the previous one). */
        private fun forecast(h: JSONObject?, daily: JSONObject?, hourNow: String): Forecast? {
            if (h == null) return null
            val times = h.getJSONArray("time")
            val first = (0 until times.length()).firstOrNull { times.getString(it).take(13) >= hourNow } ?: return null
            val n = minOf(HOURS, times.length() - first)
            if (n == 0) return null
            val start = times.getString(first).substringAfter('T').substringBefore(':').toInt()
            val hours = (first until first + n).map { i ->
                ForecastHour(
                    tempC = num(h.getJSONArray("temperature_2m"), i).toFloat(),
                    code = num(h.getJSONArray("weather_code"), i).toInt().coerceIn(0, 255),
                    rainPct = num(h.getJSONArray("precipitation_probability"), i).toInt().coerceIn(0, 100),
                    windKmh = Math.round(num(h.getJSONArray("wind_speed_10m"), i)).toInt().coerceIn(0, 255),
                )
            }
            val rise = daily?.optJSONArray("sunrise")?.optString(0)?.let { minuteOfDay(it) } ?: 0
            val set = daily?.optJSONArray("sunset")?.optString(0)?.let { minuteOfDay(it) } ?: 0
            return Forecast(start, rise, set, hours)
        }

        private fun num(a: JSONArray, i: Int): Double = if (a.isNull(i)) 0.0 else a.getDouble(i)
    }

    fun conditions(lat: Double, lon: Double): Result<Conditions> =
        Http.get(String.format(Locale.US, URL, lat, lon), TIMEOUT_MS).mapCatching { parse(String(it)) }
}
