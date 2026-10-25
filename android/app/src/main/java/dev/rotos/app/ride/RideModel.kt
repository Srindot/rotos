package dev.rotos.app.ride

import android.graphics.Bitmap
import dev.rotos.app.link.LinkState
import dev.rotos.app.link.TransferProgress
import dev.rotos.app.map.BuildProgress
import dev.rotos.app.map.PackInfo
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.LoadedRoute
import dev.rotos.app.route.Route

enum class LinkKind { BLE, TCP }

/** Everything a ride needs to know, fixed when the ride starts. */
data class RideConfig(
    val origin: LatLon,
    val destination: LatLon,
    /** Use assets/sample_route.json instead of a routing service (offline test path). */
    val useSample: Boolean,
    /** Drive the ride from the library's ride simulator instead of GPS. */
    val demo: Boolean,
    val speedup: Float,
    val link: LinkKind,
    val host: String,
    val port: Int,
    /** Show incoming calls on ROTOS and allow the one-button SMS reply. */
    val callAlerts: Boolean = true,
    /** Forward the phone's notifications (messages, app alerts) to ROTOS. */
    val notifyAlerts: Boolean = true,
    /**
     * Google Maps navigates; ROTOS shows the Google map at the rider's GPS
     * position plus Google's turns (GUIDANCE). False = ROTOS's own route
     * engine (demo / sample rides).
     */
    val googleNav: Boolean = true,
    /** No destination: map a square around [origin] instead of a route corridor. */
    val area: Boolean = false,
) {
    /** Two configs with the same key produce the same route and map. */
    val planKey: String get() = when {
        useSample -> "sample"
        area -> "area@%.2f,%.2f".format(origin.lat, origin.lon)
        else -> "$origin->$destination"
    }
}

/** A planned route with its map pack (held inside NativeNav) and preview. */
/** [source]: the route as Google gave it (its polylines feed the ROTOS line); null for area maps' sweeps. */
data class Plan(val key: String, val route: LoadedRoute, val pack: PackInfo, val preview: Bitmap, val source: Route? = null)

data class NavSnapshot(
    val fixes: Int,
    val alongM: Float,
    val remainingM: Float,
    val remainingS: Int,
    val flags: Int,
    val eta: String,
    val speedMps: Float,
)

data class RideStatus(
    val phase: String = "idle",
    val link: LinkState = LinkState.IDLE,
    val peer: String = "",
    val plan: Plan? = null,
    val build: BuildProgress? = null,
    val transfer: TransferProgress? = null,
    val nav: NavSnapshot? = null,
    val error: String = "",
    val log: List<String> = emptyList(),
)
