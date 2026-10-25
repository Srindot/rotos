package dev.rotos.app.ride

import dev.rotos.app.notify.NoteFilter
import dev.rotos.app.notify.NotifyBus
import dev.rotos.app.notify.PhoneNote

/**
 * Phone notifications (NotifyBus) -> NOTIFY frames to ROTOS while the session
 * runs. Android re-posts a notification on every update; each new text goes
 * out once. Only the app name is logged, never the message.
 */
class NotifyForwarder(private val send: FrameSender, private val log: (String) -> Unit) {
    companion object {
        private const val REMEMBERED = 64
    }

    private val last = LinkedHashMap<String, String>()

    suspend fun run() {
        NotifyBus.notes.collect { n -> forward(n) }
    }

    private suspend fun forward(n: PhoneNote) {
        if (NoteFilter.isRepeat(last, n)) return
        last.remove(n.key)
        last[n.key] = n.title + "\n" + n.text
        if (last.size > REMEMBERED) last.remove(last.keys.first())
        send.notify(n)
        log("notification to ROTOS from ${n.app}")
    }
}
