package dev.rotos.app.ride

import android.util.Log
import dev.rotos.app.location.Fix
import dev.rotos.app.nav.Packets
import dev.rotos.app.quota.Purpose
import dev.rotos.app.route.LatLon
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import java.time.LocalTime
import java.time.format.DateTimeFormatter

/**
 * ROTOS's own route engine navigates (demo and sample rides): each fix ->
 * the C engine -> its NAV_STATE to ROTOS; the engine asking for a reroute
 * plans a new route from the rider's position.
 */
class EngineRide(private val host: RideHost, private val send: FrameSender, private val ride: ActiveRide) {
    companion object {
        private const val TAG = "ROTOS"
        private val HHMM: DateTimeFormatter = DateTimeFormatter.ofPattern("HH:mm")
    }

    private var fixes = 0

    suspend fun onFix(scope: CoroutineScope, fix: Fix) {
        if (ride.rerouting) return
        var result: EngineStep? = null
        send.numbered { s -> EngineStep.run(s, fix).also { result = it }?.frame }
        val step = result
        if (step == null) {
            host.log("engine_fix failed")
            return
        }
        val a = step.along
        val arrived = step.flags and Packets.NAV_ARRIVED != 0
        val eta = LocalTime.now().plusSeconds(a.remainingS.toLong()).format(HHMM)
        fixes++
        val snap = NavSnapshot(fixes, a.alongM, a.remainingM, a.remainingS, step.flags, eta, fix.speedMps)
        host.updateStatus { it.copy(nav = snap, phase = if (arrived) "arrived" else it.phase) }
        host.rideNotice { if (arrived) "Arrived" else "%.1f km left · arrive %s".format(a.remainingM / 1000f, eta) }
        Log.i(TAG, "NAV seq=%d along=%.0f rem=%.0f rem_s=%d flags=0x%02x".format(step.seq, a.alongM, a.remainingM, a.remainingS, step.flags))
        if (a.wantsReroute && !ride.cfg.useSample && !ride.busy) scope.launch { reroute(LatLon(fix.lat, fix.lon)) }
    }

    private suspend fun reroute(here: LatLon) {
        ride.rerouting = true
        try {
            host.log("off route: rerouting from %.5f,%.5f".format(here.lat, here.lon))
            val plan = host.replan(ride.cfg.copy(origin = here), Purpose.AUTO)
            if (plan != null && host.sendMap(plan.pack)) {
                send.prep(Packets.PREP_IDLE)
                EngineStep.reset()
                ride.routeId = plan.pack.routeId
            } else {
                ride.failed()
            }
        } finally {
            ride.rerouting = false
            host.updateStatus { it.copy(phase = "riding", error = "") }        // the ride goes on
        }
    }
}
