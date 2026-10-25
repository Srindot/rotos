package dev.rotos.app.call

import android.content.Context
import android.media.AudioManager

/**
 * Silences the ringtone of the call the rider ignored on ROTOS (B) and puts
 * it back when that call ends or is answered. Muting the ring stream needs
 * Do Not Disturb access, which Android grants with the notification access
 * ROTOS already has; without it the phone just keeps ringing.
 */
class RingerMute(context: Context, private val log: (String) -> Unit) {
    private val audio = context.getSystemService(AudioManager::class.java)
    private var muted = false

    fun mute() {
        if (muted || audio == null) return
        runCatching { audio.adjustStreamVolume(AudioManager.STREAM_RING, AudioManager.ADJUST_MUTE, 0) }
            .onSuccess { muted = true; log("ringer silenced for this call") }
            .onFailure { log("could not silence the ringer: ${it.message}") }
    }

    /** The call is over (or answered): the ringer works again. */
    fun restore() {
        if (!muted || audio == null) return
        runCatching { audio.adjustStreamVolume(AudioManager.STREAM_RING, AudioManager.ADJUST_UNMUTE, 0) }
            .onFailure { log("could not restore the ringer: ${it.message}") }
        muted = false
    }
}
