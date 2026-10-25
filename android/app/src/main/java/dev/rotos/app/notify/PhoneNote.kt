package dev.rotos.app.notify

import android.app.Notification

/** One phone notification, as ROTOS shows it. */
data class PhoneNote(val key: String, val app: String, val title: String, val text: String, val message: Boolean) {
    /** 16-bit id for ROTOS: an updated notification (new chat line) keeps its id. */
    val id: Int get() = key.hashCode() and 0xFFFF
}

/**
 * Which notifications go to ROTOS, as pure rules: real messages and app
 * alerts, not ongoing ones (music, navigation, downloads), group summaries,
 * calls (the call popup does those) or the system. Pure.
 */
object NoteFilter {
    private val SKIP_PACKAGES = setOf(
        "dev.rotos.app", "com.google.android.apps.maps", "android", "com.android.systemui",
        "com.google.android.dialer", "com.android.dialer", "com.android.server.telecom", "com.android.phone",
        "com.motorola.dialer", "com.android.incallui",
    )
    private val SKIP_CATEGORIES = setOf(
        Notification.CATEGORY_CALL, Notification.CATEGORY_NAVIGATION, Notification.CATEGORY_TRANSPORT,
        Notification.CATEGORY_PROGRESS, Notification.CATEGORY_SERVICE, Notification.CATEGORY_SYSTEM,
        Notification.CATEGORY_STATUS,
    )

    fun wanted(pkg: String, ongoing: Boolean, groupSummary: Boolean, category: String?, title: String, text: String): Boolean =
        pkg !in SKIP_PACKAGES && !ongoing && !groupSummary && category !in SKIP_CATEGORIES &&
            (title.isNotBlank() || text.isNotBlank())

    fun isMessage(category: String?): Boolean =
        category == Notification.CATEGORY_MESSAGE || category == Notification.CATEGORY_EMAIL

    /** Same notification with the same text again (Android re-posts on every update): skip it. */
    fun isRepeat(last: Map<String, String>, note: PhoneNote): Boolean = last[note.key] == note.title + "\n" + note.text
}
