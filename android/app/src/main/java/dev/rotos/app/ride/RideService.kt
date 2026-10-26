package dev.rotos.app.ride

import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import androidx.core.content.ContextCompat
import dev.rotos.app.call.CallController
import dev.rotos.app.link.LinkState
import dev.rotos.app.link.MapTransfer
import dev.rotos.app.link.ReconnectingLink
import dev.rotos.app.location.CurrentLocation
import dev.rotos.app.location.DemoLocationSource
import dev.rotos.app.location.Fix
import dev.rotos.app.location.GpsLocationSource
import dev.rotos.app.location.LocationSource
import dev.rotos.app.map.PackInfo
import dev.rotos.app.map.MapPackBuilder
import dev.rotos.app.BuildConfig
import dev.rotos.app.map.BlockCache
import dev.rotos.app.map.PackBase
import dev.rotos.app.map.LinkWire
import dev.rotos.app.mirror.GuidanceBus
import dev.rotos.app.nav.Ack
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import dev.rotos.app.nav.Packets
import dev.rotos.app.net.AppIdentity
import dev.rotos.app.quota.Purpose
import dev.rotos.app.quota.UsageGuard
import dev.rotos.app.route.LatLon
import dev.rotos.app.route.Route
import kotlinx.coroutines.flow.conflate
import dev.rotos.app.util.AppLog
import dev.rotos.app.util.AppPresence
import dev.rotos.app.mirror.MapsNotificationListener
import kotlinx.coroutines.CancellationException
import java.io.File
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.time.LocalTime
import java.time.format.DateTimeFormatter

/**
 * The ride: a foreground service that owns the link and wires the modules in
 * this fixed order (docs/CODING_STANDARD.md, "RideService wires them together"):
 *
 *   session (from the moment the switch is on, independent of rides):
 *   1. link       LinkOpener (BLE or TCP, reconnects by itself) + LinkOpener.route (ACK / BUTTON / CALL_ACTION)
 *   2. dashboard  Dashboard (PHONE_STATUS, WEATHER, FORECAST, PING), GuidanceMirror (Google Maps turns),
 *                 CallController (calls, if enabled), auto ride when Google Maps starts navigating
 *   ride (Start ride / share / Google Maps; Stop returns to the session, ROTOS to its dashboard):
 *   3. plan       RoutePlanner: route -> map pack, progress to ROTOS as PREP_STATUS at 1 Hz
 *   4. map        MapTransfer, retried after a reconnect
 *   5. location   GPS or demo fixes -> MapRide (Google Maps navigates) or EngineRide (ROTOS's engine)
 *
 * Every billed Google call goes through UsageGuard (quota/).
 */
class RideService : Service(), RideHost {
    companion object {
        /** Start action after an app update (PackageReplacedReceiver): restore the session. */
        const val ACTION_RESUME = "dev.rotos.app.RESUME_SESSION"
        private const val PREP_MS = 1_000L
        /** A reroute waits at most this long for Google Maps to finish its own "Rerouting...". */
        private const val REROUTE_TARGET_WAIT_MS = 3_000L
        /** Map saved ahead of time: ~0.7 km either side of the route (zoom 16), ~150 tiles per new 20 km. */
        private const val WARM_RADIUS_PX = 300
        /** Google Maps quiet this long (rerouting, app switch) before an auto ride ends. */
        private const val AUTO_END_MS = 45_000L
        /** The last ride's fix stands for "here" only this long (then Android's last known position). */
        private const val FRESH_FIX_S = 120.0
        /** How often a running ride checks Google still navigates to its destination. */
        private const val DEST_CHECK_MS = 30_000L
        private const val LOCATE_MS = 90_000L
        private const val MAP_TRIES = 4
        private const val NOTIFY_EVERY_MS = 10_000L
        private const val GOODBYE_MS = 300L
        private const val LOG_LINES = 40
        private val CLOCK: DateTimeFormatter = DateTimeFormatter.ofPattern("HH:mm:ss")
    }

    inner class LocalBinder : Binder() {
        val service: RideService get() = this@RideService
    }

    /** What is up while the switch is on: the link and what talks over it. */
    private class Session(val link: ReconnectingLink, val send: FrameSender, val mirror: GuidanceMirror) {
        var calls: CallController? = null
    }

    private val binder = LocalBinder()
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val _status = MutableStateFlow(RideStatus())
    val status: StateFlow<RideStatus> = _status
    override val context: Context get() = this

    private val acks = Channel<Ack>(64, BufferOverflow.DROP_OLDEST)
    private val packBase by lazy { PackBase(filesDir) }
    override val lineAcks = Channel<Ack>(16, BufferOverflow.DROP_OLDEST)
    private var session: Session? = null
    private var sessionJob: Job? = null
    private var sessionKey: String? = null
    private var sessionReady = CompletableDeferred<Session?>()
    private var planJob: Job? = null
    private var warmJob: Job? = null
    private var rideJob: Job? = null
    private var ride: ActiveRide? = null
    private var locating: Job? = null
    @Volatile private var lastFix: Fix? = null
    @Volatile private var autoRide = false
    @Volatile private var autoFails = 0                 // auto-starts since one last worked
    @Volatile private var autoTriedMs = 0L
    @Volatile private var tipShown = false
    @Volatile private var destCheckMs = 0L
    @Volatile private var userStopped = false        // mirrored in RouteMemory.stopped
    private var lastNotifyMs = 0L

    override fun onCreate() {
        super.onCreate()
        AppLog.init(this)
        AppIdentity.init(this)
        UsageGuard.init(this)
        userStopped = RouteMemory(this).stopped           // stopped before a restart: stay stopped
    }

    /** Whether the foreground service holds the location type (GPS can run). */
    @Volatile private var locationFgs = true

    private fun foreground(text: String) {
        locationFgs = RideNotifications.foreground(this, text)
    }

    /** The app's screen is open: now Android allows the location type (GPS after a background restart). */
    override fun onBind(intent: Intent?): IBinder {
        if (!locationFgs && sessionJob?.isActive == true) {
            foreground(if (_status.value.link == LinkState.CONNECTED) "ROTOS connected" else "Connecting to ROTOS…")
            if (locationFgs) log("GPS restored (app opened)")
        }
        return binder
    }

    override fun onRebind(intent: Intent?) {
        onBind(intent)
    }

    override fun onUnbind(intent: Intent?): Boolean = true          // onRebind on the next bind

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        foreground("Connecting to ROTOS…")
        // null intent: Android restarted us after killing the app; ACTION_RESUME: after an update
        if ((intent == null || intent.action == ACTION_RESUME) && sessionJob?.isActive != true) {
            val cfg = SessionMemory(this).load()
            if (cfg == null) {
                stopSelf()
            } else {
                log("app restarted: reconnecting to ROTOS (a Google Maps navigation resumes the ride)")
                if (!locationFgs) log("Android refused GPS in the background: open the ROTOS app once to restore it")
                MapsNotificationListener.bindAgain(this)
                ensureSession(cfg, persistent = true)       // ROTOS may be off (ignition): keep looking
            }
        }
        return START_STICKY                                  // killed mid-ride: Android brings us back
    }

    override fun onDestroy() {
        stopRide(byUser = false)                         // the shared route stays remembered
        closeSession()
        scope.cancel()
        super.onDestroy()
    }

    // ---- public API (MainActivity, through the binder) ------------------------

    fun plan(cfg: RideConfig) {
        if (planJob?.isActive == true || rideJob?.isActive == true) return
        planJob = scope.launch { ensurePlan(cfg, Purpose.USER) }
    }

    /**
     * Keeps ROTOS connected while no ride runs (clock, weather, calls on its
     * dashboard). A no-op if already connected over the same link, or connecting.
     */
    fun connect(cfg: RideConfig) {
        ensureSession(cfg)
    }

    /** Ends the session: ROTOS goes back to its idle screen. */
    fun disconnect() {
        SessionMemory(this).clear()                          // switched off: no reconnecting after restarts
        stopRide()
        setStopped(false)                                    // switching off is not "hide Google's turns next time"
        val s = session
        val job = sessionJob
        _status.update { it.copy(phase = "idle") }
        scope.launch {
            if (s != null) {
                s.send.prep(Packets.PREP_IDLE)
                delay(GOODBYE_MS)                                // let the summary / idle frames leave
            }
            if (sessionJob !== job) return@launch               // switched on again meanwhile: leave that one alone
            closeSession()
            _status.update { it.copy(phase = "idle", link = LinkState.DISCONNECTED) }
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()                                           // only after the goodbye frames
        }
    }

    /**
     * Starts a ride; a running one is replaced quietly (no summary on ROTOS in
     * between). [auto]: started by Google Maps navigating (ends with it; its
     * Google calls count as automatic).
     */
    fun startRide(cfg: RideConfig, auto: Boolean = false) {
        if (rideJob?.isActive == true) {
            rideJob?.cancel()
            log("ride replaced")
        }
        val r = ActiveRide(cfg, if (auto) Purpose.AUTO else Purpose.USER)
        ride = r
        autoRide = auto && cfg.area                          // a ride the rider started never ends on its own
        setStopped(false)                                // a new ride: Google's turns show again
        // the rider's own share starts the 3 h memory; an automatic resume must not keep renewing it
        if (!auto && cfg.googleNav && !cfg.area && !cfg.useSample && !cfg.demo) RouteMemory(this).save(cfg.destination)
        rideJob = scope.launch {
            runCatching { runRide(r) }.onFailure { e -> if (e !is CancellationException) fail("ride: ${e.message}") }
        }
    }

    /** Ends the ride; the link stays up and ROTOS shows the ride summary, then its dashboard. */
    fun stopRide(byUser: Boolean = true) {
        val wasRiding = rideJob?.isActive == true
        if (byUser && GuidanceBus.now.value != null) {
            setStopped(true)                                  // no auto restart, no Google turns on ROTOS
            log("ride stopped: Google Maps' turns hidden on ROTOS until its navigation ends")
        }
        if (byUser) RouteMemory(this).forget()
        autoRide = false
        warmJob?.cancel()
        rideJob?.cancel()
        rideJob = null
        val r = ride
        ride = null
        if (!wasRiding || r == null) return
        val fix = lastFix
        val s = session
        if (r.reached && fix != null && s != null) {                          // ROTOS: ride summary
            scope.launch { s.send.position(fix, r.routeId, Packets.NAV_GPS_OK or Packets.NAV_ARRIVED, null, 0f, 0) }
        }
        _status.update { it.copy(phase = "idle", nav = null, transfer = null, build = null) }
        log("ride stopped")
        s?.let { scope.launch { it.send.prep(Packets.PREP_IDLE) } }
        if (sessionJob?.isActive == true) foreground("ROTOS connected")
    }

    // ---- the session -----------------------------------------------------------

    /** [persistent]: keep searching for ROTOS (restored sessions); else a few tries, then the rider decides. */
    private fun ensureSession(cfg: RideConfig, persistent: Boolean = false): CompletableDeferred<Session?> {
        val link = if (cfg.link == LinkKind.TCP) "tcp ${cfg.host}:${cfg.port}" else "ble"
        val key = "$link calls=${cfg.callAlerts} notify=${cfg.notifyAlerts}"   // a changed switch: new session
        if (sessionJob?.isActive == true && sessionKey == key) return sessionReady
        SessionMemory(this).save(cfg)
        closeSession()
        val ready = CompletableDeferred<Session?>()
        sessionKey = key
        sessionReady = ready
        runCatching { ContextCompat.startForegroundService(this, Intent(this, RideService::class.java)) }
            .onFailure { log("foreground service refused: ${it.message}") }
        AppPresence.sessionActive = true
        sessionJob = scope.launch {
            runCatching { runSession(cfg, ready, persistent) }.onFailure { e -> if (e !is CancellationException) log("session: ${e.message}") }
            if (!ready.isCompleted) ready.complete(null)
            sessionEnded()
        }
        return ready
    }

    private fun closeSession() {
        locating?.cancel()                                   // a pending "Google navigates" start: not any more
        sessionJob?.cancel()
        sessionJob = null
        sessionKey = null
        session?.link?.close()
        session = null
        sessionEnded()
    }

    /** No ROTOS session: if the app's screen is closed too, nothing keeps running. */
    private fun sessionEnded() {
        AppPresence.sessionActive = sessionJob?.isActive == true
        MapsNotificationListener.releaseIfIdle()
    }

    private suspend fun runSession(cfg: RideConfig, ready: CompletableDeferred<Session?>, persistent: Boolean) = coroutineScope {
        val l = LinkOpener.open(this@RideService, this, cfg, ::log, persistent,
            onTry = { _status.update { it.copy(link = LinkState.CONNECTING) } },
            onUp = {
                foreground("ROTOS connected")
                ride?.lineLost = true                    // a reconnect: ROTOS may have restarted
            })
        if (l == null) {
            _status.update { it.copy(link = LinkState.FAILED) }
            ready.complete(null)
            stopForeground(STOP_FOREGROUND_REMOVE)
            return@coroutineScope
        }
        val send = FrameSender(l)
        val s = Session(l, send, GuidanceMirror(send, { userStopped }, ::log))
        session = s
        ready.complete(s)
        launch {
            l.state.collect { st ->
                _status.update { it.copy(link = st, peer = l.peer) }
                if (st == LinkState.CONNECTING) foreground("Reconnecting to ROTOS…")
            }
        }
        launch { LinkOpener.route(l, acks, lineAcks, { s.calls }, ::log) }
        launch { Dashboard(this@RideService, send, ::here, ::log).run(cfg.origin) }
        launch { s.mirror.run() }
        launch { autoRideLoop(cfg) }
        if (cfg.callAlerts) startCalls(this, s) else log("call alerts disabled")
        if (cfg.notifyAlerts) launch { NotifyForwarder(send, ::log).run() } else log("notifications to ROTOS disabled")
    }

    private fun startCalls(sessionScope: CoroutineScope, s: Session) {
        val c = CallController(this, { f -> s.send.callState(f) }, { rideEta() }, ::log)
        s.calls = c
        c.start(sessionScope)
    }

    /** Where the rider is: the ride's last fix, else Android's last known position. */
    private fun here(): LatLon? = lastFix?.takeIf { System.currentTimeMillis() / 1000.0 - it.unixS < FRESH_FIX_S }
        ?.let { LatLon(it.lat, it.lon) } ?: CurrentLocation.lastKnown(this)

    /** Local arrival time for the SMS reply: Google Maps', else the engine's while riding. */
    private suspend fun rideEta(): LocalTime? {
        GuidanceBus.now.value?.eta?.let { return it }
        if (_status.value.phase != "riding" || ride?.cfg?.googleNav != false) return null   // engine rides only
        val f = FloatArray(2)
        val i = IntArray(3)
        val st = NavThread.run { NativeNav.engineState(f, i) }
        if (st < 0 || i[1] and Packets.NAV_ARRIVED != 0) return null
        return LocalTime.now().plusSeconds(i[0].toLong())
    }

    /**
     * "Just navigate in Google Maps": Google Maps starting to navigate starts
     * a ROTOS map ride around the rider; Google Maps quiet for [AUTO_END_MS]
     * ends it with the summary. A ride the rider stopped stays stopped until
     * this Google navigation ends.
     */
    private suspend fun autoRideLoop(cfg: RideConfig) {
        var sawNavigation = false                       // after a restart Google is "quiet" until it posts
        var quietSince = 0L
        GuidanceBus.now.collectLatest { g ->
            val now = System.currentTimeMillis()
            if (g == null) {
                if (!sawNavigation) return@collectLatest
                if (quietSince == 0L) quietSince = now
                delay(AUTO_END_MS)                      // cancelled if a turn arrives meanwhile
                navigationOver()
                return@collectLatest
            }
            if (quietSince != 0L && now - quietSince >= AUTO_END_MS) navigationOver()   // a new navigation
            quietSince = 0L
            sawNavigation = true
            if (rideJob?.isActive == true && now - destCheckMs > DEST_CHECK_MS) {
                destCheckMs = now
                ride?.takeIf { it.cfg.googleNav && !it.cfg.area && !it.areaFallback }?.let { r ->
                    val at = here()
                    if (at != null && ResumeRule.resume(SharedRoute(r.cfg.destination, now), now, at, g.remainingM) == null) {
                        log("Google Maps navigates somewhere else now: ride replaced")
                        stopRide(byUser = false)             // the next update starts the new ride
                    }
                }
            }
            if (rideJob?.isActive == true || userStopped || cfg.demo) return@collectLatest
            if (autoFails > 0 && now - autoTriedMs < autoBackoffMs()) return@collectLatest   // it failed: not per update
            autoFails++
            autoTriedMs = now
            val at = here()
            if (at != null) autoStart(cfg, at) else locateThenStart(cfg)
        }
    }

    /** Google Maps' navigation is over: stopped / tip / retry state belong to it. */
    private fun navigationOver() {
        setStopped(false)
        tipShown = false
        autoFails = 0
        // every Google-mirrored ride ends with Google's navigation (arrived / stopped): no GPS, no
        // reroutes to an old destination while the rider rides on with the phone in the pocket
        if ((autoRide || ride?.cfg?.googleNav == true) && rideJob?.isActive == true) {
            log("Google Maps navigation ended: ride over")
            stopRide(byUser = false)
        }
    }

    /** After a failed auto-start: 1, 2, 4 ... 10 min before the next try. */
    private fun autoBackoffMs(): Long = minOf(60_000L shl minOf(autoFails - 1, 4), 600_000L)

    private fun locateThenStart(cfg: RideConfig) {
        if (locating?.isActive == true) return                  // ask once, start when the fix arrives
        log("Google Maps navigating: waiting for a location fix")
        locating = scope.launch {
            val fix = CurrentLocation.get(this@RideService, LOCATE_MS)
            if (fix == null) log("no location fix: ROTOS keeps the turn screen only")
            else if (GuidanceBus.now.value != null && rideJob?.isActive != true && !userStopped &&
                sessionJob?.isActive == true) autoStart(cfg, fix)
        }
    }

    private fun setStopped(on: Boolean) {
        if (userStopped == on) return
        userStopped = on
        RouteMemory(this).stopped = on
    }

    private fun autoStart(cfg: RideConfig, at: LatLon) {
        val dest = ResumeRule.resume(RouteMemory(this).load(), System.currentTimeMillis(), at,
            GuidanceBus.now.value?.remainingM)
        if (dest != null) {                              // app restarted mid-ride: keep the blue line
            log("Google Maps is navigating: resuming the shared route to %.5f,%.5f".format(dest.lat, dest.lon))
            startRide(cfg.copy(origin = at, destination = dest, area = false, googleNav = true, useSample = false), auto = true)
            return
        }
        log("Google Maps is navigating: ROTOS map ride around %.5f,%.5f".format(at.lat, at.lon))
        if (!tipShown) {                                 // once per Google navigation, not per retry
            tipShown = true
            RideNotifications.askForDestination(this)
        }
        startRide(cfg.copy(origin = at, destination = at, area = true, googleNav = true, useSample = false), auto = true)
    }

    // ---- the ride ----------------------------------------------------------------

    // Connect first, so ROTOS shows the phone's planning progress (PREP_STATUS)
    // instead of "WAITING FOR PHONE" while tiles download.
    private suspend fun runRide(r: ActiveRide) = coroutineScope {
        val cfg = r.cfg
        _status.update { it.copy(phase = "connecting", error = "") }
        val s = ensureSession(cfg).await()                                     // 1 + 2
        if (s == null) {
            fail(if (cfg.link == LinkKind.BLE) "ROTOS not found. Is it powered on and within a few metres?"
                 else "Simulator not reachable at ${cfg.host}:${cfg.port}")
            return@coroutineScope
        }
        if (!s.link.awaitConnected()) {
            fail("ROTOS link is down")
            return@coroutineScope
        }
        var plan = replan(cfg, r.purpose)                                       // 3
        if (plan == null && _status.value.error.contains(MapPackBuilder.TOO_LONG) && cfg.googleNav && !cfg.area) {
            // too long for one map: Google's turns + the map around the rider, refreshed as they ride
            log("route too long for one ROTOS map: riding with the map around you instead")
            r.areaFallback = true
            plan = replan(cfg.copy(origin = cfg.origin, destination = cfg.origin, area = true), r.purpose)
        }
        if (plan == null) return@coroutineScope
        if (!sendMap(plan.pack)) return@coroutineScope                  // 4
        s.send.prep(Packets.PREP_IDLE)
        _status.update { it.copy(phase = "riding") }
        r.routeId = plan.pack.routeId
        r.reached = true
        r.line = cfg.googleNav && !cfg.area && !r.areaFallback                 // Google's route: draw it
        r.coverage = plan.route.points
        if (!cfg.googleNav) EngineStep.reset()                                 // ROTOS's own engine: a fresh match
        val source: LocationSource = if (cfg.demo) DemoLocationSource(cfg.speedup) else GpsLocationSource(this@RideService)
        log("location: ${source.name}" + (if (cfg.demo) " x${cfg.speedup}" else "") +
            if (cfg.googleNav) " (Google Maps navigates, ROTOS shows the map)" else "")
        val mapRide = if (cfg.googleNav) MapRide(this@RideService, s.send, r) else null
        val engineRide = if (cfg.googleNav) null else EngineRide(this@RideService, s.send, r)
        mapRide?.start(plan)                                                   // the line, as ROUTE packets
        // conflate: a fix that waited behind a slow one is stale, only the newest counts
        source.fixes().conflate().collect { fix ->                            // 5
            if (lastFix == null || autoFails != 0) autoFails = 0                 // a fix arrived: the auto-start worked
            lastFix = fix
            mapRide?.onFix(this, fix)
            engineRide?.onFix(this, fix)
        }
        if (_status.value.phase == "riding") _status.update { it.copy(phase = "ride finished") }
        log("location source finished; link stays up")
    }

    /** Plans (or waits for a "Plan route" already running) while forwarding progress to ROTOS at 1 Hz. */
    override suspend fun replan(cfg: RideConfig, purpose: Purpose, route: Route?): Plan? = coroutineScope {
        val send = session?.send
        val forward = launch {
            while (isActive) {
                _status.value.build?.let { b -> send?.prep(b.prep, b.pct, b.done, b.total) }
                delay(PREP_MS)
            }
        }
        planJob?.join()
        val plan = ensurePlan(cfg, purpose, route)
        forward.cancel()
        if (plan == null) send?.prep(Packets.PREP_FAILED) else send?.prep(Packets.PREP_READY, 100)
        plan
    }

    override fun warmMap(points: List<LatLon>) {
        warmJob?.cancel()                                    // the newest route matters; saved blocks stay saved
        warmJob = scope.launch(Dispatchers.Default) {
            val t0 = System.currentTimeMillis()
            val (n, tiles) = runCatching {
                MapPackBuilder(cacheDir, BuildConfig.MAPS_API_KEY, BlockCache.of(filesDir), File(filesDir, "tiles"))
                    .warm(points, WARM_RADIUS_PX)
            }.getOrElse { e -> if (e is CancellationException) throw e else 0 to 0 }
            if (n > 0) log("map around the route saved ahead: $n blocks, $tiles Google tiles downloaded, in ${(System.currentTimeMillis() - t0) / 1000} s")
        }
    }

    override suspend fun findRoute(cfg: RideConfig, purpose: Purpose): Result<Route> {
        val target = session?.mirror?.target(REROUTE_TARGET_WAIT_MS)       // reroutes: never wait long
        return RoutePlanner(this) { all, best, t -> log(PlanLog.picked(all, best, t)) }.route(cfg, cfg.origin, target, purpose)
    }

    private suspend fun ensurePlan(cfg: RideConfig, purpose: Purpose, route: Route? = null): Plan? {
        val cur = _status.value.plan
        // the cached plan only for the same trip AND the same route (a reroute's map is a different route)
        if (cur != null && cur.key == cfg.planKey && (route == null || cur.source === route) &&
            NavThread.run { NativeNav.packRouteId() } == cur.pack.routeId) return cur
        _status.update { it.copy(phase = "planning", error = "", build = null, transfer = null, nav = null) }
        log("planning ${cfg.planKey}")
        val target = if (cfg.googleNav && !cfg.area && route == null) session?.mirror?.target() else null
        val planner = RoutePlanner(this) { all, best, t -> log(PlanLog.picked(all, best, t)) }
        return planner.plan(cfg, cfg.origin, { p -> _status.update { it.copy(build = p) } }, target, purpose, route).fold(
            onSuccess = { p ->
                PlanLog.planned(p).forEach(::log)
                _status.update { it.copy(phase = "planned", plan = p) }
                p
            },
            onFailure = { e -> fail("planning failed: ${e.message}"); null },
        )
    }

    /** The map transfer, tried again after a reconnect if the link dropped in the middle. */
    override suspend fun sendMap(pack: PackInfo): Boolean {
        val s = session ?: return false.also { fail("could not send the map to ROTOS: no link") }
        for (attempt in 1..MAP_TRIES) {
            if (!s.link.awaitConnected()) break
            if (transfer(s, pack, last = attempt == MAP_TRIES)) return true
            log("map transfer interrupted; retrying when ROTOS is back")
        }
        fail("could not send the map to ROTOS")
        return false
    }

    /**
     * One map transfer: as a patch against the map ROTOS holds when that is
     * smaller (overlapping maps shrink to a few KB), else plain deflate; a
     * ROTOS without that base gets the whole map at once.
     */
    private suspend fun transfer(s: Session, pack: PackInfo, last: Boolean): Boolean {
        _status.update { it.copy(phase = "sending map", transfer = null) }
        val base = packBase.get()?.takeIf { it.crc != pack.crc32 }
        val (wire, patched) = NavThread.run { LinkWire.prepare(pack.length, base) }
        log("map transfer: $wire bytes on the link (pack ${pack.length}${if (patched) ", a patch of the previous map" else ""})")
        val t0 = System.currentTimeMillis()
        var r = sendWire(s, wire)
        if (r.exceptionOrNull() is MapTransfer.NoBase) {
            log("ROTOS holds another map: sending the whole map")
            r = sendWire(s, NavThread.run { LinkWire.plain(pack.length) })
        }
        return r.fold(
            onSuccess = {
                log("map transfer OK in ${(System.currentTimeMillis() - t0) / 1000.0} s")
                NavThread.run { packBase.remember(pack.length, pack.crc32) }
                true
            },
            onFailure = { e ->
                if (last) fail("map transfer failed: ${e.message}") else log("map transfer failed: ${e.message}")
                false
            },
        )
    }

    private suspend fun sendWire(s: Session, wire: Int): Result<Unit> =
        MapTransfer(s.link, s.send.seq, acks).send(wire) { p ->
            s.send.markSent()
            _status.update { it.copy(transfer = p) }
        }

    // ---- RideHost helpers -------------------------------------------------------

    override fun updateStatus(f: (RideStatus) -> RideStatus) = _status.update(f)

    override fun rideNotice(text: () -> String) {
        val now = System.currentTimeMillis()
        if (now - lastNotifyMs < NOTIFY_EVERY_MS || session?.link?.state?.value != LinkState.CONNECTED) return
        lastNotifyMs = now
        foreground(text())
    }

    override fun log(msg: String) {
        AppLog.i(msg)
        val line = LocalTime.now().format(CLOCK) + "  " + msg
        _status.update { it.copy(log = (it.log + line).takeLast(LOG_LINES)) }
    }

    private fun fail(msg: String) {
        log(msg)
        _status.update { it.copy(phase = "error", error = msg) }
    }
}
