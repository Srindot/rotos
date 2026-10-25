package dev.rotos.app.notify

import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow

/** Phone notifications from the listener to whoever forwards them (RideService). */
object NotifyBus {
    private val _notes = MutableSharedFlow<PhoneNote>(extraBufferCapacity = 16, onBufferOverflow = BufferOverflow.DROP_OLDEST)
    val notes: SharedFlow<PhoneNote> = _notes

    fun post(note: PhoneNote) {
        _notes.tryEmit(note)
    }
}
