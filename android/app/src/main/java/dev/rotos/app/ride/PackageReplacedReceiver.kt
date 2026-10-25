package dev.rotos.app.ride

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import androidx.core.content.ContextCompat

/** An app update killed the running app: reconnect to ROTOS if the rider had it on. */
class PackageReplacedReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_MY_PACKAGE_REPLACED || SessionMemory(context).load() == null) return
        runCatching {
            ContextCompat.startForegroundService(context, Intent(context, RideService::class.java).setAction(RideService.ACTION_RESUME))
        }
    }
}
