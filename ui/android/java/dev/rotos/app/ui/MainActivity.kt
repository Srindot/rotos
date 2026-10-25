package dev.rotos.app.ui

import android.bluetooth.BluetoothManager
import android.content.ActivityNotFoundException
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.location.LocationManager
import android.net.Uri
import android.os.Bundle
import android.os.IBinder
import android.provider.Settings
import android.view.View
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.location.LocationManagerCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.google.android.material.chip.Chip
import com.google.android.material.snackbar.Snackbar
import dev.rotos.app.BuildConfig
import dev.rotos.app.R
import dev.rotos.app.location.CurrentLocation
import dev.rotos.app.mirror.GuidanceBus
import dev.rotos.app.mirror.MapsNotificationListener
import dev.rotos.app.ride.LinkKind
import dev.rotos.app.ride.RideConfig
import dev.rotos.app.ride.RideService
import dev.rotos.app.ride.RideStatus
import dev.rotos.app.route.LatLon
import dev.rotos.app.share.Destination
import dev.rotos.app.share.DestinationResolver
import dev.rotos.app.quota.UsageGuard
import dev.rotos.app.util.AppLog
import dev.rotos.app.util.AppPresence
import dev.rotos.app.util.Permissions
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

/**
 * The one screen. Places are always picked in Google Maps: either the rider
 * navigates there as usual and ROTOS mirrors the turns (MapsNotificationListener
 * -> GUIDANCE), or shares a place to ROTOS for a ride with the ROTOS map.
 * All work happens in RideService; this class collects inputs and renders
 * RideStatus (UiModel turns the status into what each card shows).
 *
 * Test hooks (adb shell am start ... --ez autostart true): extras dest, origin,
 * demo, sample, calls, link (tcp|ble), host, port, speedup, connect, autoplan, autostart.
 */
class MainActivity : AppCompatActivity() {
    private companion object {
        /** Map packs fit routes up to about this far (2.25 MB flash partition). */
        const val MAX_RIDE_KM = 60
    }

    private lateinit var v: MainViews
    private lateinit var screen: RideScreen
    private var routeMap: RouteMap? = null
    private lateinit var recents: Recents

    private var service: RideService? = null
    private var collectJob: Job? = null
    private var pendingAuto: String? = null
    /** Set when the field holds a resolved place (from a share or a recent chip). */
    private var picked: Destination? = null
    private val lastStatus: RideStatus get() = screen.lastStatus

    private val permLauncher = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        if (v.connectSwitch.isChecked) connectRotos()
    }
    private val btLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (v.connectSwitch.isChecked) connectRotos()
    }
    /** A place shared before the service was bound; ridden to as soon as it is. */
    private var pendingShared: Destination? = null
    /** Destination from the test hook (extra "dest"), resolved when the ride starts. */
    private var pendingDest: String? = null

    private val conn = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, b: IBinder?) {
            val svc = (b as RideService.LocalBinder).service
            service = svc
            collectJob = lifecycleScope.launch {
                repeatOnLifecycle(Lifecycle.State.STARTED) { svc.status.collect { screen.render(it) } }
            }
            pendingShared?.let { pendingShared = null; startSharedRide(it); return }
            if (pendingAuto == null && v.connectSwitch.isChecked) connectRotos()
            pendingAuto?.let { runAuto(it) }
            pendingAuto = null
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            service = null
            collectJob?.cancel()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        AppLog.init(this)
        UsageGuard.init(this)
        setContentView(R.layout.activity_main)
        v = MainViews(this)
        screen = RideScreen(this, v) { routeMap }
        recents = Recents(this)
        setupMap(savedInstanceState)
        setupInputs()
        if (savedInstanceState == null) {                 // not again after rotation / theme change
            applyExtras(intent)
            handleShare(intent)
        }
        renderRecents()
        requestPermissions()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        applyExtras(intent)
        handleShare(intent)
        if (service != null) {
            pendingAuto?.let { runAuto(it) }
            pendingAuto = null
        }
    }

    // ---- inputs ---------------------------------------------------------------

    private fun setupInputs() {
        v.start.setOnClickListener { onStartStop() }
        v.plan.setOnClickListener { if (readyToRide()) withDestination { service?.startRide(it) } }
        v.googleMaps.setOnClickListener { openGoogleMaps() }
        v.callAlerts.setOnCheckedChangeListener { _, on -> if (on) requestPermissions() }
        v.notifyAlerts.setOnCheckedChangeListener { _, on ->            // needs notification access (same as Google Maps turns)
            if (on && !MapsNotificationListener.hasAccess(this)) openListenerSettings()
            if (v.connectSwitch.isChecked) connectRotos()               // new setting: new session
        }
        v.advancedHeader.setOnClickListener { toggleAdvanced() }
        v.linkChip.setOnClickListener { v.connectSwitch.isChecked = true }
        v.connectSwitch.setOnCheckedChangeListener { _, on ->
            if (screen.syncingSwitch) return@setOnCheckedChangeListener
            if (on) connectRotos() else service?.disconnect()
        }
        v.linkGroup.addOnButtonCheckedListener { _, _, checked -> if (checked && v.connectSwitch.isChecked) connectRotos() }
        v.mirrorAccess.setOnClickListener { openListenerSettings() }
        v.goingTo.setOnClickListener { setPicked(null) }
        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.STARTED) { GuidanceBus.now.collect { screen.renderMirror() } }
        }
        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.STARTED) { UsageGuard.usage.collect { UsageCard.render(v, it) } }
        }
    }

    private fun onStartStop() {
        if (UiModel.isActive(lastStatus.phase)) {
            service?.stopRide()
            return
        }
        if (!v.connectSwitch.isChecked) v.connectSwitch.isChecked = true   // the ride needs ROTOS linked
        openGoogleMaps()                                     // ROTOS follows once Google navigates
    }

    /** Bluetooth on (for the BLE link) and location on (for a real ride); else explain. */
    private fun readyToRide(): Boolean {
        if (Permissions.missing(this, v.callAlerts.isChecked).isNotEmpty()) requestPermissions()
        if (v.linkBle.isChecked) {
            val bt = getSystemService(BluetoothManager::class.java)?.adapter
            if (bt != null && !bt.isEnabled) {
                runCatching { btLauncher.launch(Intent(android.bluetooth.BluetoothAdapter.ACTION_REQUEST_ENABLE)) }
                    .onFailure { snack(getString(R.string.bt_off), Settings.ACTION_BLUETOOTH_SETTINGS) }
                return false
            }
        }
        if (!v.demo.isChecked) {
            val lm = getSystemService(LocationManager::class.java)
            if (lm != null && !LocationManagerCompat.isLocationEnabled(lm)) {
                snack(getString(R.string.location_off), Settings.ACTION_LOCATION_SOURCE_SETTINGS)
                return false
            }
        }
        return true
    }

    /**
     * The rider switched "Connect to ROTOS" on: link up in the background
     * (clock, weather, calls on its dashboard). Asks for Bluetooth / the
     * permissions first when they are missing; the switch result retries.
     */
    private fun connectRotos() {
        val svc = service ?: return
        if (UiModel.isActive(lastStatus.phase)) return                // a ride owns the link
        if (v.linkBle.isChecked) {
            if (Permissions.missing(this, false).isNotEmpty()) {
                requestPermissions()
                return
            }
            val bt = getSystemService(BluetoothManager::class.java)?.adapter ?: return
            if (!bt.isEnabled) {
                runCatching { btLauncher.launch(Intent(android.bluetooth.BluetoothAdapter.ACTION_REQUEST_ENABLE)) }
                    .onFailure { snack(getString(R.string.bt_off), Settings.ACTION_BLUETOOTH_SETTINGS) }
                return
            }
        }
        val here = CurrentLocation.lastKnown(this) ?: DestinationResolver.IIIT_HYDERABAD
        svc.connect(buildConfig(here, here))
    }

    private fun requestPermissions() {
        val missing = Permissions.missing(this, v.callAlerts.isChecked)
        if (missing.isNotEmpty()) permLauncher.launch(missing.toTypedArray())
    }

    private fun toggleAdvanced() {
        val open = v.advancedBody.visibility != View.VISIBLE
        v.advancedBody.visibility = if (open) View.VISIBLE else View.GONE
        v.advancedChevron.animate().rotation(if (open) 180f else 0f).setDuration(150).start()
    }

    // ---- destination ------------------------------------------------------------

    /**
     * The chosen place (recent / shared / sample) -> a ride config. No place,
     * or one too far for a map pack: ROTOS maps the area around the rider and
     * Google Maps does the rest.
     */
    private fun withDestination(action: (RideConfig) -> Unit) {
        note(null)
        lifecycleScope.launch {
            val chosen = picked
            val hook = pendingDest
            val dest: Destination? = when {
                chosen != null -> chosen
                v.sample.isChecked -> Destination("Durgam Cheruvu", DestinationResolver.DURGAM_CHERUVU)
                hook != null -> DestinationResolver(this@MainActivity).resolvePlace(hook).getOrElse {
                    note(it.message, error = true)
                    return@launch
                }
                else -> null
            }
            AppLog.i("destination: " + (dest?.let { "${it.label} @ ${it.at}" } ?: "none (area map)"))
            if (dest != null && !v.sample.isChecked) setPicked(dest)
            val from = origin() ?: run {
                snack(getString(R.string.no_gps_fix), Settings.ACTION_LOCATION_SOURCE_SETTINGS)
                return@launch
            }
            val km = dest?.let { (DestinationResolver.distanceM(from, it.at) / 1000.0).toInt() } ?: 0
            val area = dest == null || (!v.sample.isChecked && km > MAX_RIDE_KM)
            if (dest != null && area) note(getString(R.string.too_far_area, dest.label, km))
            if (dest != null && !v.sample.isChecked) recents.add(dest)
            renderRecents()
            action(buildConfig(from, dest?.at ?: from, area))
        }
    }

    /**
     * A place shared from Google Maps: ROTOS rides there with Google's route as
     * the blue line (map + line sent to ROTOS), and Google Maps navigates.
     */
    private fun startSharedRide(d: Destination) {
        if (service == null) {                               // opened by the share: wait for the service
            pendingShared = d
            return
        }
        if (!v.connectSwitch.isChecked) v.connectSwitch.isChecked = true
        if (!readyToRide()) return                       // a running ride is replaced by startRide
        withDestination { cfg ->
            service?.startRide(cfg)
            openGoogleNavigation(cfg.destination)
        }
    }

    /**
     * Google Maps in two-wheeler navigation to [to] (mode=l), or simply opened
     * for the rider to pick a place there when [to] is null.
     */
    private fun openGoogleNavigation(to: LatLon?) {
        if (to == null) {
            openGoogleMaps()
            return
        }
        val nav = Intent(Intent.ACTION_VIEW, Uri.parse("google.navigation:q=${to.lat},${to.lon}&mode=l"))
            .setPackage(MapsNotificationListener.MAPS)
        try {
            startActivity(nav)
        } catch (e: ActivityNotFoundException) {
            snack(getString(R.string.maps_not_installed), null)
        }
    }

    /** A line under the Google Maps buttons: progress / error, or hidden. */
    private fun note(msg: String?, error: Boolean = false) {
        v.destNote.visibility = if (msg.isNullOrEmpty()) View.GONE else View.VISIBLE
        v.destNote.text = msg
        v.destNote.setTextColor(ContextCompat.getColor(this, if (error) R.color.ctp_red else R.color.ctp_subtext0))
    }

    /** ACTION_SEND text from Google Maps "Share", or ACTION_VIEW geo: / maps URLs. */
    private fun handleShare(i: Intent?) {
        val text = when (i?.action) {
            Intent.ACTION_SEND -> i.getStringExtra(Intent.EXTRA_TEXT)
            Intent.ACTION_VIEW -> i.dataString
            else -> null
        } ?: return
        AppLog.i("shared to ROTOS: " + text.replace('\n', ' ').take(160))
        note(getString(R.string.resolving_link))
        lifecycleScope.launch {
            DestinationResolver(this@MainActivity).resolvePlace(text).onSuccess { d ->
                AppLog.i("shared place resolved: ${d.label} @ ${d.at}")
                note(null)
                setPicked(d)
                startSharedRide(d)
            }.onFailure {
                AppLog.i("share failed: ${it.message}")
                note(it.message, error = true)
            }
        }
    }

    private fun openGoogleMaps() {
        val i = packageManager.getLaunchIntentForPackage(MapsNotificationListener.MAPS)
        try {
            startActivity(i ?: Intent(Intent.ACTION_VIEW, Uri.parse("geo:0,0?q=")))
        } catch (e: ActivityNotFoundException) {
            snack(getString(R.string.maps_not_installed), null)
        }
    }

    /** Sets (or clears) the shared place for a ROTOS map ride, shown as a "ROTOS map ride to …" line. */
    private fun setPicked(d: Destination?) {
        picked = d
        if (d == null) {
            v.goingTo.visibility = View.GONE
            screen.renderAction()
            return
        }
        val here = CurrentLocation.lastKnown(this)
        val dist = here?.let { " · %.1f km away".format(DestinationResolver.distanceM(it, d.at) / 1000.0) }.orEmpty()
        val detail = d.detail.ifBlank { d.at.toString() }
        v.goingTo.text = getString(R.string.going_to, d.label) + "\n" + detail + dist + " · " + getString(R.string.tap_to_clear)
        v.goingTo.visibility = View.VISIBLE
        screen.renderAction()
    }

    private fun openListenerSettings() {
        val component = ComponentName(this, MapsNotificationListener::class.java)
        val detail = Intent("android.settings.NOTIFICATION_LISTENER_DETAIL_SETTINGS")
            .putExtra("android.provider.extra.NOTIFICATION_LISTENER_COMPONENT_NAME", component.flattenToString())
        runCatching { startActivity(detail) }
            .recoverCatching { startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) }
    }

    private fun renderRecents() {
        val list = if (v.advancedBody.visibility == View.VISIBLE) recents.list() else emptyList()
        v.recentTitle.visibility = if (list.isEmpty()) View.GONE else View.VISIBLE
        v.recentChips.visibility = v.recentTitle.visibility
        v.recentChips.removeAllViews()
        list.forEach { d ->
            val chip = Chip(this).apply {
                text = d.label.take(28)
                chipIcon = ContextCompat.getDrawable(context, R.drawable.ic_place)
                setChipIconTintResource(R.color.ctp_mauve)
                setOnClickListener { setPicked(d) }
            }
            v.recentChips.addView(chip)
        }
    }

    /** Typed start, else (demo / sample) IIIT Hyderabad, else where the phone is now. */
    private suspend fun origin(): LatLon? {
        DestinationResolver.parseLatLon(v.origin.text.toString())?.let { return it }
        if (v.demo.isChecked || v.sample.isChecked) return DestinationResolver.IIIT_HYDERABAD
        note(getString(R.string.getting_location))
        val here = CurrentLocation.get(this)
        note(null)
        return here
    }

    private fun buildConfig(from: LatLon, to: LatLon, area: Boolean = false): RideConfig {
        val (host, port) = v.hostPort.text.toString().split(':').let { p ->
            (p.getOrNull(0)?.ifBlank { null } ?: "10.0.2.2") to (p.getOrNull(1)?.toIntOrNull() ?: 7400)
        }
        return RideConfig(
            origin = from, destination = to, useSample = v.sample.isChecked, demo = v.demo.isChecked,
            speedup = v.speedup.text.toString().toFloatOrNull()?.coerceIn(0.1f, 50f) ?: 1f,
            link = if (v.linkBle.isChecked) LinkKind.BLE else LinkKind.TCP, host = host, port = port,
            callAlerts = v.callAlerts.isChecked,
            notifyAlerts = v.notifyAlerts.isChecked,
            googleNav = !v.demo.isChecked,                   // demo rides keep ROTOS's own route engine
            area = area && !v.sample.isChecked,
        )
    }

    private fun snack(msg: String, settingsAction: String?) {
        val sb = Snackbar.make(v.root, msg, Snackbar.LENGTH_LONG)
        if (settingsAction != null) sb.setAction(R.string.open_settings) { startActivity(Intent(settingsAction)) }
        sb.show()
    }

    // ---- map, test hooks, lifecycle -------------------------------------------------------

    private fun setupMap(state: Bundle?) {
        if (BuildConfig.MAPS_API_KEY.isBlank()) {
            v.mapView.visibility = View.GONE
            v.mapNote.visibility = View.VISIBLE
            return
        }
        v.mapNote.visibility = View.GONE
        v.mapView.visibility = View.VISIBLE
        v.preview.visibility = View.GONE
        routeMap = RouteMap(v.mapView, this).also { it.onCreate(state?.getBundle("map")) }
    }

    private fun applyExtras(i: Intent?) {
        val e = i?.extras ?: return
        if (i.action == Intent.ACTION_SEND) return
        e.getString("dest")?.let { pendingDest = it }
        e.getString("origin")?.let { v.origin.setText(it) }
        if (e.containsKey("demo")) v.demo.isChecked = e.getBoolean("demo")
        if (e.containsKey("sample")) v.sample.isChecked = e.getBoolean("sample")
        if (e.containsKey("calls")) v.callAlerts.isChecked = e.getBoolean("calls")
        if (e.containsKey("notify")) v.notifyAlerts.isChecked = e.getBoolean("notify")
        e.getString("link")?.let { v.linkGroup.check(if (it == "ble") R.id.linkBle else R.id.linkTcp) }
        val host = e.getString("host")
        if (host != null) v.hostPort.setText("$host:${e.getInt("port", 7400)}")
        if (e.containsKey("speedup")) v.speedup.setText(e.getFloat("speedup").toString())
        if (e.getBoolean("connect")) v.connectSwitch.isChecked = true
        pendingAuto = when {
            e.getBoolean("autostart") -> "start"
            e.getBoolean("autoplan") -> "plan"
            else -> null
        }
    }

    private fun runAuto(what: String) =
        withDestination { cfg -> if (what == "start") service?.startRide(cfg) else service?.plan(cfg) }

    override fun onStart() {
        super.onStart()
        AppPresence.screenOpen = true
        MapsNotificationListener.bindAgain(this)          // Google Maps turns and notifications again
        bindService(Intent(this, RideService::class.java), conn, Context.BIND_AUTO_CREATE)
        routeMap?.onStart()
    }

    override fun onResume() {
        super.onResume()
        routeMap?.onResume()
    }

    override fun onPause() {
        routeMap?.onPause()
        super.onPause()
    }

    override fun onStop() {
        routeMap?.onStop()
        collectJob?.cancel()
        runCatching { unbindService(conn) }
        service = null
        if (!isChangingConfigurations) {                  // not for a rotation / theme change
            AppPresence.screenOpen = false
            MapsNotificationListener.releaseIfIdle()      // no ride, not connected: the app stops completely
        }
        super.onStop()
    }

    override fun onDestroy() {
        routeMap?.onDestroy()
        super.onDestroy()
    }

    @Deprecated("Deprecated in Java")
    override fun onLowMemory() {
        super.onLowMemory()
        routeMap?.onLowMemory()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        routeMap?.let { m -> outState.putBundle("map", Bundle().also { m.onSaveInstanceState(it) }) }
    }
}
