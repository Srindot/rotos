package dev.rotos.app.location

import android.annotation.SuppressLint
import android.content.Context
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.Looper
import kotlinx.coroutines.channels.awaitClose
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.callbackFlow

/**
 * The phone's position at ~1 Hz, from Android's fused provider (GPS + Wi-Fi +
 * cell, what Google Maps uses: it keeps working indoors and at a standstill,
 * where the raw GPS chip often has no fix), with GPS and network as well.
 * Starts with the last known position so ROTOS can show its map at once.
 * Permission is requested by the UI.
 */
class GpsLocationSource(private val context: Context) : LocationSource {
    override val name = "location (fused + GPS + network)"

    @SuppressLint("MissingPermission")
    override fun fixes(): Flow<Fix> = callbackFlow {
        val lm = context.getSystemService(LocationManager::class.java)
        if (lm == null) {
            close(IllegalStateException("no LocationManager"))
            return@callbackFlow
        }
        var lastMs = 0L
        var lastFix: Fix? = null
        var lastSentWall = 0L
        var lastRealWall = System.currentTimeMillis()
        val listener = LocationListener { loc ->
            // several providers report: keep newer fixes, but a rough network fix may not
            // replace a recent accurate one (it would jump the rider hundreds of metres)
            val prev = lastFix
            val rough = prev != null && loc.accuracy > maxOf(3f * prev.accuracyM, 60f) &&
                loc.time - lastMs < 5_000L
            if (!rough && (loc.time > lastMs + 200 || (loc.time >= lastMs && loc.accuracy < 20f))) {
                lastMs = loc.time
                val f = toFix(loc)
                lastFix = f
                lastSentWall = System.currentTimeMillis()
                lastRealWall = lastSentWall
                trySend(f)
            }
        }
        val providers = buildList {
            if (Build.VERSION.SDK_INT >= 31 && lm.allProviders.contains(FUSED)) add(FUSED)
            add(LocationManager.GPS_PROVIDER)
            add(LocationManager.NETWORK_PROVIDER)
        }.filter { p -> runCatching { lm.isProviderEnabled(p) }.getOrDefault(false) }
        if (providers.isEmpty()) {
            close(IllegalStateException("location is off"))
            return@callbackFlow
        }
        providers.mapNotNull { runCatching { lm.getLastKnownLocation(it) }.getOrNull() }
            .filter { System.currentTimeMillis() - it.time < MAX_AGE_MS }
            .maxByOrNull { it.time }
            ?.let { lastMs = it.time; lastFix = toFix(it); lastSentWall = System.currentTimeMillis(); trySend(lastFix!!) }
        for (p in providers) runCatching { request(lm, p, listener) }
        // Standing still, Android may not report for a while: repeat the last
        // position every second (speed 0) so ROTOS knows the phone is there.
        val beat = launch {
            while (isActive) {
                delay(1000L)
                val now = System.currentTimeMillis()
                val last = lastFix
                // standing still: repeat; no real fix for 10 s (tunnel): stop, ROTOS shows WAITING FOR GPS.
                // Never while moving: a "speed 0" repeat of an old position would drag ROTOS back.
                val still = last != null && (last.speedMps < 0f || last.speedMps < STILL_MPS)
                if (last != null && still && now - lastSentWall > 1500L && now - lastRealWall < 10_000L) {
                    val repeat = last.copy(tMs = (android.os.SystemClock.elapsedRealtime()).toInt(), unixS = now / 1000.0,
                        speedMps = 0f)
                    lastSentWall = now
                    trySend(repeat)
                }
            }
        }
        awaitClose {
            beat.cancel()
            lm.removeUpdates(listener)
        }
    }

    @SuppressLint("MissingPermission")
    private fun request(lm: LocationManager, provider: String, listener: LocationListener) {
        if (Build.VERSION.SDK_INT >= 31) {
            // high accuracy, as often as the receiver can (twice a second where supported)
            val req = android.location.LocationRequest.Builder(INTERVAL_MS)
                .setQuality(android.location.LocationRequest.QUALITY_HIGH_ACCURACY)
                .setMinUpdateIntervalMillis(0L)
                .build()
            lm.requestLocationUpdates(provider, req, context.mainExecutor, listener)
        } else {
            lm.requestLocationUpdates(provider, INTERVAL_MS, 0f, listener, Looper.getMainLooper())
        }
    }

    companion object {
        private const val INTERVAL_MS = 500L
        private const val STILL_MPS = 0.8f
        private const val FUSED = "fused"                // LocationManager.FUSED_PROVIDER (API 31)
        private const val MAX_AGE_MS = 5 * 60_000L

        fun toFix(l: Location): Fix = Fix(
            tMs = (l.elapsedRealtimeNanos / 1_000_000L).toInt(),
            unixS = l.time / 1000.0,
            lat = l.latitude,
            lon = l.longitude,
            speedMps = if (l.hasSpeed()) l.speed else -1f,
            headingDeg = if (l.hasBearing()) l.bearing else -1f,
            accuracyM = if (l.hasAccuracy()) l.accuracy else 50f,
        )
    }
}
