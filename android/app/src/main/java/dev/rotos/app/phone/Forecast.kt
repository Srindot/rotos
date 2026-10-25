package dev.rotos.app.phone

/** One forecast hour (FORECAST payload entry). */
data class ForecastHour(val tempC: Float, val code: Int, val rainPct: Int, val windKmh: Int)

/** Inputs of a FORECAST packet (rn_protocol.h RnForecast): up to 8 hours from now. */
data class Forecast(
    /** Local hour of hours[0]. */
    val startHour: Int,
    val sunriseMin: Int,
    val sunsetMin: Int,
    val hours: List<ForecastHour>,
)

/** Current conditions plus the hourly forecast, from one Open-Meteo request. */
data class Conditions(val now: Weather, val forecast: Forecast?)
