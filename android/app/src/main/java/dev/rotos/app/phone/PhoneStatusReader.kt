package dev.rotos.app.phone

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build
import android.telephony.TelephonyManager
import java.time.ZonedDateTime

/** Battery, charging, signal bars (best effort) and local time. */
class PhoneStatusReader(private val context: Context) {
    fun read(now: ZonedDateTime = ZonedDateTime.now()): PhoneStatus {
        val bm = context.getSystemService(BatteryManager::class.java)
        val pct = bm?.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY) ?: -1
        val sticky = runCatching {
            context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        }.getOrNull()
        val plugged = (sticky?.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) ?: 0) != 0
        return PhoneStatus(
            timeOfDayS = now.toLocalTime().toSecondOfDay(),
            weekday = now.dayOfWeek.value - 1,
            day = now.dayOfMonth,
            month = now.monthValue,
            batteryPct = pct.coerceIn(0, 100),
            charging = plugged || (bm?.isCharging ?: false),
            signalBars = signalBars(),
        )
    }

    private fun signalBars(): Int {
        if (Build.VERSION.SDK_INT < 28) return 0
        val tm = context.getSystemService(TelephonyManager::class.java) ?: return 0
        return runCatching { tm.signalStrength?.level ?: 0 }.getOrDefault(0).coerceIn(0, 4)
    }
}
