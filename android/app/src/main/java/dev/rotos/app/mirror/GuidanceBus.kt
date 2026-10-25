package dev.rotos.app.mirror

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

/**
 * The latest turn from Google Maps (null = not navigating), written by
 * MapsNotificationListener, read by RideService (-> GUIDANCE packets) and the UI.
 */
object GuidanceBus {
    private val _now = MutableStateFlow<Guidance?>(null)
    val now: StateFlow<Guidance?> = _now
    /** The raw notification text of the last update, for the Advanced log. */
    @Volatile var lastRaw: String = ""

    fun publish(g: Guidance?, raw: String) {
        lastRaw = raw
        _now.value = g
    }
}
