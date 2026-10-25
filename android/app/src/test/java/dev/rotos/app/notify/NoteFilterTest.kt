package dev.rotos.app.notify

import android.app.Notification
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NoteFilterTest {
    @Test fun messagesGoToRotos() {
        assertTrue(NoteFilter.wanted("com.whatsapp", false, false, Notification.CATEGORY_MESSAGE, "Amma", "Where are you?"))
        assertTrue(NoteFilter.wanted("com.google.android.apps.messaging", false, false, null, "HDFC", "Rs 450 spent"))
    }

    @Test fun noiseStaysOnThePhone() {
        assertFalse(NoteFilter.wanted("com.spotify.music", true, false, Notification.CATEGORY_TRANSPORT, "Song", "Artist"))
        assertFalse(NoteFilter.wanted("com.whatsapp", false, true, Notification.CATEGORY_MESSAGE, "2 chats", "3 messages"))
        assertFalse(NoteFilter.wanted("com.google.android.dialer", false, false, Notification.CATEGORY_CALL, "Amma", "Incoming"))
        assertFalse(NoteFilter.wanted("com.google.android.apps.maps", false, false, null, "Traffic", "Heavy"))
        assertFalse(NoteFilter.wanted("dev.rotos.app", false, false, null, "ROTOS", "connected"))
        assertFalse(NoteFilter.wanted("com.whatsapp", false, false, Notification.CATEGORY_MESSAGE, " ", ""))
    }

    @Test fun updatesWithTheSameTextAreRepeats() {
        val n = PhoneNote("k1", "WhatsApp", "Amma", "Hi", true)
        assertTrue(NoteFilter.isRepeat(mapOf("k1" to "Amma\nHi"), n))
        assertFalse(NoteFilter.isRepeat(mapOf("k1" to "Amma\nHello"), n))
        assertEquals(n.id, n.copy(text = "new line").id)                 // same notification keeps its id
    }
}
