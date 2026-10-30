package dev.rotos.app.mirror

import android.app.Notification
import android.content.ComponentName
import android.content.Context
import android.provider.Settings
import android.service.notification.NotificationListenerService
import android.service.notification.NotificationListenerService.Ranking
import android.service.notification.StatusBarNotification
import dev.rotos.app.util.AppLog
import dev.rotos.app.util.AppPresence
import dev.rotos.app.BuildConfig
import dev.rotos.app.notify.NoteReader
import dev.rotos.app.notify.NotifyBus
import java.time.LocalTime

/**
 * The app's notification listener ("notification access", granted once in
 * Settings): Google Maps' turn-by-turn notification (the same source
 * smartwatches use) goes to GuidanceBus; other apps' new notifications
 * (messages, alerts; NoteFilter) go to NotifyBus for ROTOS.
 */
class MapsNotificationListener : NotificationListenerService() {
    private var activeKey: String? = null

    companion object {
        @Volatile private var current: MapsNotificationListener? = null

        /** Nothing needs the app (screen closed, no ROTOS session): let Android unbind and stop us. */
        fun releaseIfIdle() {
            if (!AppPresence.needed) current?.let { runCatching { it.requestUnbind() } }
        }

        /** The app is wanted again: ask Android to bind the listener (no-op when bound). */
        fun bindAgain(ctx: Context) {
            runCatching { requestRebind(ComponentName(ctx, MapsNotificationListener::class.java)) }
        }

        const val MAPS = "com.google.android.apps.maps"
        private val GUIDANCE_TITLE = Regex("""^(\d+(?:[.,]\d+)?\s*(m|km|ft|mi)\b|starting navigation|rerouting)""", RegexOption.IGNORE_CASE)

        fun hasAccess(ctx: Context): Boolean {
            val me = ComponentName(ctx, MapsNotificationListener::class.java).flattenToString()
            val list = Settings.Secure.getString(ctx.contentResolver, "enabled_notification_listeners").orEmpty()
            return list.split(':').any { it == me }
        }
    }

    override fun onCreate() {
        super.onCreate()
        AppLog.init(this)
    }

    override fun onListenerConnected() {
        current = this
        if (!AppPresence.needed) {                       // bound at boot with nothing to do
            requestUnbind()
            return
        }
        AppLog.i("gmaps listener connected")
        activeNotifications?.filter { isNavigation(it) }?.forEach { onNotificationPosted(it) }
    }

    /**
     * Would the phone itself alert for this one? Silent / muted channels, apps the
     * rider turned down and anything Do Not Disturb holds back stay off ROTOS too.
     */
    private fun alerts(sbn: StatusBarNotification): Boolean = runCatching {
        val r = Ranking()
        if (!currentRanking.getRanking(sbn.key, r)) return@runCatching true
        r.importance >= android.app.NotificationManager.IMPORTANCE_DEFAULT && r.matchesInterruptionFilter()
    }.getOrDefault(true)

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        if (!isNavigation(sbn)) {
            if (alerts(sbn)) NoteReader.read(this, sbn)?.let { NotifyBus.post(it) }
            return
        }
        activeKey = sbn.key
        val e = sbn.notification.extras
        val title = e.getCharSequence(Notification.EXTRA_TITLE)?.toString().orEmpty()
        val text = (e.getCharSequence(Notification.EXTRA_BIG_TEXT) ?: e.getCharSequence(Notification.EXTRA_TEXT))
            ?.toString().orEmpty()
        val summary = listOfNotNull(
            e.getCharSequence(Notification.EXTRA_SUB_TEXT),
            e.getCharSequence(Notification.EXTRA_INFO_TEXT),
            e.getCharSequence(Notification.EXTRA_SUMMARY_TEXT),
        ).joinToString(" · ")
        val icon = arrow(sbn.notification)
        val raw = "title=[$title] text=[$text] sub=[$summary] icon=" + (icon?.let { "%016x".format(it.first) } ?: "-")
        val g = GuidanceParser.parse(title, text, summary, LocalTime.now(), icon?.second)
        if (raw != GuidanceBus.lastRaw) AppLog.i("gmaps $raw -> $g")
        GuidanceBus.publish(g, raw)
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification) {
        if (!isNavigation(sbn) || (activeKey != null && sbn.key != activeKey)) return
        activeKey = null
        AppLog.i("gmaps navigation ended")
        GuidanceBus.publish(null, "")
    }

    override fun onListenerDisconnected() {
        current = null
        GuidanceBus.publish(null, "")
    }

    /**
     * The turn arrow (large icon) -> (hash, maneuver). Each new arrow is saved
     * once as icons/<hash>.png next to the log, to check the classifier against
     * Google's real icons.
     */
    private fun arrow(n: Notification): Pair<Long, Int?>? = runCatching {
        val d = n.getLargeIcon()?.loadDrawable(this) ?: return null
        val size = 64
        val bmp = android.graphics.Bitmap.createBitmap(size, size, android.graphics.Bitmap.Config.ARGB_8888)
        d.setBounds(0, 0, size, size)
        d.draw(android.graphics.Canvas(bmp))
        val px = IntArray(size * size)
        bmp.getPixels(px, 0, size, 0, 0, size, size)
        val hash = ArrowClassifier.hash(px, size, size)
        val m = KnownArrows.lookup(hash) ?: ArrowClassifier.classify(px, size, size)
        saveIcon(hash, bmp, m)
        hash to m
    }.getOrNull()

    private fun saveIcon(hash: Long, bmp: android.graphics.Bitmap, m: Int?) {
        val dir = java.io.File(getExternalFilesDir(null) ?: filesDir, "icons")
        val f = java.io.File(dir, "%016x.png".format(hash))
        if (f.exists()) return
        dir.mkdirs()
        f.outputStream().use { bmp.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, it) }
        AppLog.i("gmaps new arrow icon %016x -> maneuver %s".format(hash, m?.toString() ?: "?"))
    }

    /**
     * Google Maps' ongoing navigation notification (not its other notifications).
     * Debug builds also take `adb shell cmd notification post -t TITLE gmaps-test TEXT`
     * (posted by com.android.shell), for tests without Google Maps installed.
     */
    private fun isNavigation(sbn: StatusBarNotification): Boolean {
        if (BuildConfig.DEBUG && sbn.packageName == "com.android.shell" && sbn.tag?.startsWith("gmaps-test") == true) {
            return true
        }
        if (sbn.packageName != MAPS) return false
        val n = sbn.notification
        if (n.category == Notification.CATEGORY_NAVIGATION) return true
        // older Maps builds without a category: ongoing, and the title looks like guidance
        val title = n.extras.getCharSequence(Notification.EXTRA_TITLE)?.toString().orEmpty()
        return sbn.isOngoing && GUIDANCE_TITLE.containsMatchIn(title)
    }
}
