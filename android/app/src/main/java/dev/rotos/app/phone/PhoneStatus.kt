package dev.rotos.app.phone

/** Inputs of a PHONE_STATUS packet (rn_protocol.h RnPhoneStatus). */
data class PhoneStatus(
    val timeOfDayS: Int,
    /** 0 = Monday */
    val weekday: Int,
    val day: Int,
    val month: Int,
    val batteryPct: Int,
    val charging: Boolean,
    val signalBars: Int,
)

/** Inputs of a WEATHER packet (rn_protocol.h RnWeather). */
data class Weather(
    val tempC: Float,
    val code: Int,
    val rainPct: Int,
    val windKmh: Int,
    val isDay: Boolean,
)
