package dev.rotos.app.ride

import android.Manifest
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import dev.rotos.app.R
import dev.rotos.app.util.AppLog
import dev.rotos.app.mirror.MapsNotificationListener
import dev.rotos.app.ui.MainActivity

/** The ride's ongoing notification and the one tip it shows. */
object RideNotifications {
    private const val CHANNEL = "ride"
    private const val NOTIFY_ID = 1
    private const val TIP_CHANNEL = "share_tip"     // heads-up; Android keeps an old channel's importance
    private const val TIP_ID = 2
    private const val TIP_TIMEOUT_MS = 120_000L

    /** Keeps [service] in the foreground with [text] as its status line. */
    fun foreground(service: Service, text: String) {
        val nm = service.getSystemService(NotificationManager::class.java)
        nm?.createNotificationChannel(NotificationChannel(CHANNEL, "Ride", NotificationManager.IMPORTANCE_LOW))
        val open = PendingIntent.getActivity(service, 0, Intent(service, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val n: Notification = NotificationCompat.Builder(service, CHANNEL)
            .setSmallIcon(R.drawable.ic_notify)
            .setContentTitle("ROTOS")
            .setContentText(text)
            .setContentIntent(open)
            .setOngoing(true)
            .build()
        if (Build.VERSION.SDK_INT < 29) {
            service.startForeground(NOTIFY_ID, n)
            return
        }
        // Restarted in the background (after a kill / an update) Android may refuse the
        // location type: stay connected to ROTOS anyway, location follows when allowed.
        runCatching { service.startForeground(NOTIFY_ID, n, foregroundTypes(service)) }
            .recoverCatching { service.startForeground(NOTIFY_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE) }
            .onFailure { AppLog.i("foreground refused: ${it.message}") }
    }

    /** Google navigates but ROTOS does not know where to: suggest the share for the blue line. */
    fun askForDestination(context: Context) {
        val nm = context.getSystemService(NotificationManager::class.java) ?: return
        nm.createNotificationChannel(NotificationChannel(TIP_CHANNEL, "Blue line tip", NotificationManager.IMPORTANCE_HIGH))
        val maps = context.packageManager.getLaunchIntentForPackage(MapsNotificationListener.MAPS) ?: return
        val open = PendingIntent.getActivity(context, 1, maps, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val n = NotificationCompat.Builder(context, TIP_CHANNEL)
            .setSmallIcon(R.drawable.ic_notify)
            .setContentTitle("Blue line on ROTOS")
            .setContentText("In Google Maps tap your destination → Share → ROTOS")
            .setStyle(NotificationCompat.BigTextStyle().bigText(
                "ROTOS shows the map, but Google Maps does not tell other apps where you are going. " +
                    "In Google Maps tap your destination → Share → ROTOS: ROTOS draws Google's route and starts the navigation for you."))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setContentIntent(open)
            .setAutoCancel(true)
            .setTimeoutAfter(TIP_TIMEOUT_MS)
            .build()
        runCatching { nm.notify(TIP_ID, n) }
    }

    private fun foregroundTypes(context: Context): Int {
        var t = ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
        val fine = ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION)
        if (fine == PackageManager.PERMISSION_GRANTED) t = t or ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
        return t
    }
}
