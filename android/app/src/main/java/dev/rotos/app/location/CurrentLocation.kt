package dev.rotos.app.location

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.location.Location
import android.location.LocationManager
import androidx.core.content.ContextCompat
import androidx.core.location.LocationManagerCompat
import androidx.core.os.CancellationSignal
import dev.rotos.app.route.LatLon
import dev.rotos.app.util.Permissions
import kotlin.coroutines.resume
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeoutOrNull

/**
 * Where the rider is now: the freshest last-known position from any provider
 * (GPS, fused, network), else one fresh fix.
 */
object CurrentLocation {
    private const val MAX_AGE_MS = 30 * 60_000L

    @SuppressLint("MissingPermission")
    fun lastKnown(ctx: Context): LatLon? {
        if (!Permissions.has(ctx, Manifest.permission.ACCESS_FINE_LOCATION) &&
            !Permissions.has(ctx, Manifest.permission.ACCESS_COARSE_LOCATION)) return null
        val lm = ctx.getSystemService(LocationManager::class.java) ?: return null
        val now = System.currentTimeMillis()
        val best: Location = lm.getProviders(true)
            .mapNotNull { runCatching { lm.getLastKnownLocation(it) }.getOrNull() }
            .filter { now - it.time < MAX_AGE_MS }
            .maxByOrNull { it.time } ?: return null
        return LatLon(best.latitude, best.longitude)
    }

    /** Last known, else waits up to [timeoutMs] for one fresh GPS / network fix; null if none. */
    @SuppressLint("MissingPermission")
    suspend fun get(ctx: Context, timeoutMs: Long = 20_000L): LatLon? {
        lastKnown(ctx)?.let { return it }
        if (!Permissions.has(ctx, Manifest.permission.ACCESS_FINE_LOCATION)) return null
        val lm = ctx.getSystemService(LocationManager::class.java) ?: return null
        val provider = listOf(LocationManager.GPS_PROVIDER, LocationManager.NETWORK_PROVIDER)
            .firstOrNull { runCatching { lm.isProviderEnabled(it) }.getOrDefault(false) } ?: return null
        return withTimeoutOrNull(timeoutMs) {
            suspendCancellableCoroutine { cont ->
                val cancel = CancellationSignal()
                cont.invokeOnCancellation { cancel.cancel() }
                LocationManagerCompat.getCurrentLocation(lm, provider, cancel, ContextCompat.getMainExecutor(ctx)) { loc ->
                    cont.resume(loc?.let { LatLon(it.latitude, it.longitude) })
                }
            }
        }
    }
}
