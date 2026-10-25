package dev.rotos.app.ride

import android.content.Context
import dev.rotos.app.map.PackInfo
import dev.rotos.app.nav.Ack
import dev.rotos.app.quota.Purpose
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.Route
import kotlinx.coroutines.channels.ReceiveChannel

/** What a ride needs from the service that runs it (RideService). */
interface RideHost {
    val context: Context
    fun log(msg: String)
    fun updateStatus(f: (RideStatus) -> RideStatus)
    /** The ongoing notification's text, at most every 10 s and only while ROTOS is connected. */
    fun rideNotice(text: () -> String)
    /**
     * Plans [cfg] with progress on ROTOS; null when planning failed. With
     * [route], only the map is built for that route (no Routes call).
     */
    suspend fun replan(cfg: RideConfig, purpose: Purpose, route: Route? = null): Plan?
    /** Google's route only (one Routes call, matched to what Google Maps navigates), for instant reroutes. */
    suspend fun findRoute(cfg: RideConfig, purpose: Purpose): Result<Route>
    /** Sends the planned map pack, retried across reconnects; false when it never arrived. */
    suspend fun sendMap(pack: PackInfo): Boolean
    /** In the background, saves the map blocks around [points] (reroutes / detours then find them ready). */
    fun warmMap(points: List<LatLon>)
    /** ROTOS's confirmations of ROUTE lines (ACK value = line id). */
    val lineAcks: ReceiveChannel<Ack>
}
