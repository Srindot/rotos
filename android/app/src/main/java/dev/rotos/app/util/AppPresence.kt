package dev.rotos.app.util

/**
 * Whether anything needs the app right now: its screen is open, or a session
 * with ROTOS (connection, ride) runs. When neither, the notification listener
 * lets go (MapsNotificationListener.releaseIfIdle) and the app stops entirely.
 */
object AppPresence {
    @Volatile var screenOpen = false
    @Volatile var sessionActive = false

    val needed: Boolean get() = screenOpen || sessionActive
}
