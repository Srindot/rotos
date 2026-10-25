package dev.rotos.app.notify

import android.app.Notification
import android.content.Context
import android.service.notification.StatusBarNotification

/** A StatusBarNotification -> PhoneNote, or null when it is not for ROTOS (NoteFilter). */
object NoteReader {
    fun read(ctx: Context, sbn: StatusBarNotification): PhoneNote? {
        val n = sbn.notification
        val e = n.extras
        val title = (e.getCharSequence(Notification.EXTRA_CONVERSATION_TITLE) ?: e.getCharSequence(Notification.EXTRA_TITLE))
            ?.toString().orEmpty()
        val text = (e.getCharSequence(Notification.EXTRA_BIG_TEXT) ?: e.getCharSequence(Notification.EXTRA_TEXT))
            ?.toString().orEmpty()
        val summary = (n.flags and Notification.FLAG_GROUP_SUMMARY) != 0
        if (!NoteFilter.wanted(sbn.packageName, sbn.isOngoing, summary, n.category, title, text)) return null
        return PhoneNote(sbn.key, appName(ctx, sbn.packageName), title, text, NoteFilter.isMessage(n.category))
    }

    private fun appName(ctx: Context, pkg: String): String = runCatching {
        val pm = ctx.packageManager
        pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString()
    }.getOrDefault(pkg.substringAfterLast('.'))
}
