package dev.rotos.app.route

import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import java.util.zip.CRC32

/**
 * Feeds a [Route] into the C library (rn_ffi.h "typical phone flow", step 1):
 *   reset; per step: first = add_polyline, add_maneuver_google(maneuver, first, text);
 *   finish(duration, route_id).
 * The whole load runs as one NavThread task, so no fix ever sees half a route.
 */
object RouteLoader {
    private const val MAX_POINTS = 4096

    /** Non-zero id derived from the geometry (CRC-32 of all step polylines). */
    fun routeIdOf(route: Route): Int {
        val crc = CRC32()
        route.steps.forEach { crc.update(it.encodedPolyline.toByteArray()) }
        val v = crc.value.toInt()
        return if (v == 0) 1 else v
    }

    suspend fun load(route: Route): Result<LoadedRoute> = NavThread.run { loadNow(route) }

    private fun loadNow(route: Route): Result<LoadedRoute> {
        NativeNav.reset()
        for ((i, s) in route.steps.withIndex()) {
            val first = NativeNav.routeAddPolyline(s.encodedPolyline)
            if (first < 0) return fail("step $i polyline", first)
            val man = s.maneuver.ifEmpty { if (i == 0) "DEPART" else "STRAIGHT" }
            val st = NativeNav.routeAddManeuverGoogle(man, first, s.instruction)
            if (st < 0) return fail("step $i maneuver", st)
        }
        val id = routeIdOf(route)
        val st = NativeNav.routeFinish(route.durationS, id)
        if (st < 0) return fail("route finish", st)
        return Result.success(snapshot(id, route.source))
    }

    private fun snapshot(id: Int, source: String): LoadedRoute {
        val n = minOf(NativeNav.routePointCount(), MAX_POINTS)
        val pt = DoubleArray(3)
        val pts = ArrayList<LatLon>(n)
        for (i in 0 until n) {
            if (NativeNav.routePoint(i, pt) >= 0) pts.add(LatLon(pt[0], pt[1]))
        }
        val mans = ArrayList<ManeuverInfo>()
        val info = FloatArray(3)
        val road = ByteArray(NativeNav.ROAD_BYTES)
        for (i in 0 until minOf(NativeNav.routeManeuverCount(), 200)) {
            if (NativeNav.routeManeuver(i, info, road) < 0) continue
            val len = road.indexOf(0).let { if (it < 0) road.size else it }
            mans.add(ManeuverInfo(info[0].toInt(), info[1].toInt(), info[2], String(road, 0, len)))
        }
        return LoadedRoute(id, NativeNav.routeTotalM(), NativeNav.routeDurationS(), pts, mans, source)
    }

    private fun fail(what: String, code: Int): Result<LoadedRoute> =
        Result.failure(IllegalStateException("$what failed (rn_status $code)"))
}
