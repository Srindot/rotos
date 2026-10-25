package dev.rotos.app.ui

import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import dev.rotos.app.R
import dev.rotos.app.link.LinkState
import dev.rotos.app.mirror.GuidanceBus
import dev.rotos.app.mirror.MapsNotificationListener
import dev.rotos.app.ride.RideStatus

/**
 * Draws RideStatus on the main screen: one function per card, all of them
 * reading UiModel (what to show) and writing [v] (where). No logic beyond
 * picking texts and colours; no service calls.
 */
class RideScreen(private val a: AppCompatActivity, private val v: MainViews, private val routeMap: () -> RouteMap?) {
    /** The status last drawn. */
    var lastStatus = RideStatus()
        private set
    /** True while [render] moves the connect switch, so its listener ignores the change. */
    var syncingSwitch = false
        private set
    private var shownPlanKey: String? = null

    fun render(s: RideStatus) {
        lastStatus = s
        val ui = UiModel.from(s)
        renderLink(ui)
        renderProgress(ui)
        v.errorCard.visibility = if (ui.error == null) View.GONE else View.VISIBLE
        v.errorText.text = ui.error
        renderRoute(s, ui)
        renderRide(ui)
        renderAction(ui)
        v.heroCard.visibility = if (UiModel.isActive(ui.phase) || ui.error != null) View.GONE else View.VISIBLE
        val linked = ui.link == LinkState.CONNECTED
        v.heroTitle.setText(if (linked) R.string.hero_connected else R.string.hero_title)
        v.heroBody.setText(if (linked) R.string.hero_connected_body else R.string.hero_steps)
        renderConnect(ui)
        renderMirror()
        v.status.text = StatusFormatter.format(s)
        v.log.text = s.log.takeLast(14).joinToString("\n")
    }

    private fun renderLink(ui: UiModel) {
        val (text, color) = when (ui.link) {
            LinkState.CONNECTED -> R.string.link_connected to R.color.ctp_green
            LinkState.CONNECTING -> (if (ui.wasConnected) R.string.link_reconnecting else R.string.link_searching) to R.color.ctp_yellow
            LinkState.FAILED -> R.string.link_not_found to R.color.ctp_red
            else -> R.string.link_idle to R.color.ctp_overlay1
        }
        v.linkChip.setText(text)
        v.linkChip.setChipIconTintResource(color)
    }

    private fun renderConnect(ui: UiModel) {
        val on = ui.link == LinkState.CONNECTED || ui.link == LinkState.CONNECTING
        if (v.connectSwitch.isChecked != on && !(v.connectSwitch.isChecked && ui.link == LinkState.IDLE)) {
            syncingSwitch = true
            v.connectSwitch.isChecked = on
            syncingSwitch = false
        }
        val (text, color) = when (ui.link) {
            LinkState.CONNECTED -> R.string.connect_on to R.color.ctp_green
            LinkState.CONNECTING -> (if (ui.wasConnected) R.string.connect_reconnecting else R.string.connect_searching) to R.color.ctp_yellow
            LinkState.FAILED -> R.string.connect_failed to R.color.ctp_red
            else -> R.string.connect_off to R.color.ctp_overlay1
        }
        v.connectStatus.setText(text)
        v.connectIcon.setColorFilter(ContextCompat.getColor(a, color))
    }

    private fun renderProgress(ui: UiModel) {
        val p = ui.progress
        v.progressCard.visibility = if (p == null) View.GONE else View.VISIBLE
        if (p == null) return
        v.progressTitle.text = p.title
        v.progressDetail.text = p.detail
        v.progressDetail.visibility = if (p.detail.isEmpty()) View.GONE else View.VISIBLE
        if (p.pct == null) {
            v.progress.isIndeterminate = true
        } else {
            v.progress.isIndeterminate = false
            v.progress.setProgressCompat(p.pct, true)
        }
    }

    private fun renderRoute(s: RideStatus, ui: UiModel) {
        val plan = s.plan
        v.routeCard.visibility = if (plan == null) View.GONE else View.VISIBLE
        if (plan == null) return
        v.routeSource.text = ui.routeSource
        v.routeKm.text = ui.routeKm
        v.routeTime.text = ui.routeMin
        v.routeTurns.text = ui.routeTurns
        v.routeFlyover.text = ui.routeNote
        val area = plan.route.source == "area"
        v.routeKmLabel.setText(if (area) R.string.km_left else R.string.km)
        v.routeTimeLabel.setText(if (area) R.string.arrival else R.string.minutes)
        v.routeTurnsLabel.setText(if (area) R.string.next_turn else R.string.turns)
        if (plan.key + plan.pack.routeId != shownPlanKey) {
            shownPlanKey = plan.key + plan.pack.routeId
            v.preview.setImageBitmap(plan.preview)
            routeMap()?.show(plan.route.points, line = !area)      // area maps: frame them, no zigzag
        }
    }

    private fun renderRide(ui: UiModel) {
        val r = ui.ride
        v.rideCard.visibility = if (r == null) View.GONE else View.VISIBLE
        if (r == null) return
        v.rideTitle.text = r.title
        v.rideLeft.text = r.kmLeft
        v.rideEta.text = r.eta
        v.rideSpeed.text = r.speed
        v.rideFlags.text = r.flags
    }

    fun renderAction(ui: UiModel = UiModel.from(lastStatus)) {
        val active = UiModel.isActive(ui.phase)
        v.start.setText(if (active) R.string.stop_ride else R.string.open_google_maps)
        v.start.setIconResource(if (active) R.drawable.ic_stop else R.drawable.ic_map)
        val bg = if (active) R.color.ctp_red else R.color.ctp_mauve
        v.start.backgroundTintList = ContextCompat.getColorStateList(a, bg)
        v.plan.isEnabled = !active
    }


    /** What the Google Maps mirror is doing: needs access / ready / live turn. */
    fun renderMirror() {
        val access = MapsNotificationListener.hasAccess(a)
        v.mirrorAccess.visibility = if (access) View.GONE else View.VISIBLE
        val g = GuidanceBus.now.value
        v.mirrorStatus.text = when {
            !access -> a.getString(R.string.mirror_need_access)
            g != null -> a.getString(R.string.mirror_live, UiModel.describe(g))
            lastStatus.link != LinkState.CONNECTED -> a.getString(R.string.mirror_ready_offline)
            else -> a.getString(R.string.mirror_ready)
        }
    }
}
