package dev.rotos.app.ride

import android.content.Context
import dev.rotos.app.BuildConfig
import dev.rotos.app.map.BuildProgress
import dev.rotos.app.map.BlockCache
import dev.rotos.app.map.MapPackBuilder
import dev.rotos.app.map.MapPreview
import dev.rotos.app.nav.NavThread
import dev.rotos.app.nav.Packets
import dev.rotos.app.quota.Purpose
import dev.rotos.app.route.AreaRoute
import dev.rotos.app.route.GoogleTarget
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.Route
import dev.rotos.app.route.RouteMatcher
import dev.rotos.app.route.RouteLoader
import dev.rotos.app.route.RouteRepository

/** Route -> NativeNav -> map pack -> preview, in that order. */
class RoutePlanner(private val context: Context, private val onPicked: (List<Route>, Int, GoogleTarget?) -> Unit = { _, _, _ -> }) {
    private val repo = RouteRepository(BuildConfig.MAPS_API_KEY, context.assets)

    /**
     * [target]: what Google Maps is navigating (km / minutes left, announced
     * roads). When set, the candidate route closest to it is used, so the
     * line on ROTOS is Google's route, not just any route. [purpose]: who
     * asked, for the Google usage caps (reroutes are AUTO).
     */
    suspend fun plan(
        cfg: RideConfig, from: LatLon, progress: (BuildProgress) -> Unit, target: GoogleTarget? = null,
        purpose: Purpose = Purpose.USER, given: Route? = null,
    ): Result<Plan> {
        val src = when {
            given != null -> "the new route"
            cfg.useSample -> "sample route"
            cfg.area -> "the area around you"
            else -> "Google route"
        }
        progress(BuildProgress("finding route ($src)", 0, 0, Packets.PREP_ROUTE, 2))
        val route = when {
            given != null -> Result.success(given)
            cfg.useSample -> repo.sample()
            cfg.area -> Result.success(AreaRoute.around(from))
            else -> route(cfg, from, target, purpose)
        }.getOrElse { return Result.failure(it) }
        val loaded = RouteLoader.load(route).getOrElse { return Result.failure(it) }
        val buffer = if (cfg.googleNav && !cfg.area) MapPackBuilder.ROUTE_BUFFER_PX else MapPackBuilder.BUFFER_PX
        val pack = MapPackBuilder(context.cacheDir, BuildConfig.MAPS_API_KEY, BlockCache(context.filesDir), java.io.File(context.filesDir, "tiles")).build(loaded, progress, flyovers = !cfg.googleNav, bufferPx = buffer, purpose = purpose)
            .getOrElse { return Result.failure(it) }
        // An area map has no route: its sweep line only chose the tiles, so it is not drawn.
        val preview = NavThread.run { MapPreview.render(pack, if (cfg.area) emptyList() else loaded.points) }
        val key = if (cfg.useSample || cfg.area) cfg.planKey else "$from->${cfg.destination}"
        return Result.success(Plan(key, loaded, pack, preview, route))
    }

    /** Only Google's route from [from] to the destination (one Routes call, no map): reroutes. */
    suspend fun route(cfg: RideConfig, from: LatLon, target: GoogleTarget?, purpose: Purpose): Result<Route> =
        repo.alternatives(from, cfg.destination, purpose).map { all ->
            val best = RouteMatcher.pick(all, target)
            onPicked(all, best, target)
            all[best]
        }
}
