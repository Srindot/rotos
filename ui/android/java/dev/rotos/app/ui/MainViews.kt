package dev.rotos.app.ui

import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.google.android.gms.maps.MapView
import com.google.android.material.button.MaterialButton
import com.google.android.material.button.MaterialButtonToggleGroup
import com.google.android.material.card.MaterialCardView
import com.google.android.material.chip.Chip
import com.google.android.material.chip.ChipGroup
import com.google.android.material.materialswitch.MaterialSwitch
import com.google.android.material.progressindicator.LinearProgressIndicator
import com.google.android.material.textfield.TextInputEditText
import dev.rotos.app.R

/** The main screen's views, looked up once. */
class MainViews(a: AppCompatActivity) {
    val root: View = a.findViewById(android.R.id.content)
    val linkChip: Chip = a.findViewById(R.id.linkChip)
    val mirrorStatus: TextView = a.findViewById(R.id.mirrorStatus)
    val mirrorAccess: MaterialButton = a.findViewById(R.id.btnMirrorAccess)
    val destNote: TextView = a.findViewById(R.id.destNote)
    val googleMaps: MaterialButton = a.findViewById(R.id.btnGoogleMaps)
    val recentTitle: TextView = a.findViewById(R.id.recentTitle)
    val recentChips: ChipGroup = a.findViewById(R.id.recentChips)
    val heroCard: MaterialCardView = a.findViewById(R.id.heroCard)
    val heroTitle: TextView = a.findViewById(R.id.heroTitle)
    val heroBody: TextView = a.findViewById(R.id.heroBody)
    val connectSwitch: MaterialSwitch = a.findViewById(R.id.connectSwitch)
    val connectStatus: TextView = a.findViewById(R.id.connectStatus)
    val connectIcon: ImageView = a.findViewById(R.id.connectIcon)
    val goingTo: TextView = a.findViewById(R.id.goingTo)
    val progressCard: MaterialCardView = a.findViewById(R.id.progressCard)
    val progressTitle: TextView = a.findViewById(R.id.progressTitle)
    val progress: LinearProgressIndicator = a.findViewById(R.id.progress)
    val progressDetail: TextView = a.findViewById(R.id.progressDetail)
    val errorCard: MaterialCardView = a.findViewById(R.id.errorCard)
    val errorText: TextView = a.findViewById(R.id.errorText)
    val rideCard: MaterialCardView = a.findViewById(R.id.rideCard)
    val rideTitle: TextView = a.findViewById(R.id.rideTitle)
    val rideLeft: TextView = a.findViewById(R.id.rideLeft)
    val rideEta: TextView = a.findViewById(R.id.rideEta)
    val rideSpeed: TextView = a.findViewById(R.id.rideSpeed)
    val rideFlags: TextView = a.findViewById(R.id.rideFlags)
    val routeCard: MaterialCardView = a.findViewById(R.id.routeCard)
    val routeSource: Chip = a.findViewById(R.id.routeSource)
    val mapView: MapView = a.findViewById(R.id.mapView)
    val preview: ImageView = a.findViewById(R.id.preview)
    val routeKm: TextView = a.findViewById(R.id.routeKm)
    val routeTime: TextView = a.findViewById(R.id.routeTime)
    val routeTurns: TextView = a.findViewById(R.id.routeTurns)
    val routeKmLabel: TextView = a.findViewById(R.id.routeKmLabel)
    val routeTimeLabel: TextView = a.findViewById(R.id.routeTimeLabel)
    val routeTurnsLabel: TextView = a.findViewById(R.id.routeTurnsLabel)
    val routeFlyover: TextView = a.findViewById(R.id.routeFlyover)
    val callAlerts: MaterialSwitch = a.findViewById(R.id.callAlerts)
    val notifyAlerts: MaterialSwitch = a.findViewById(R.id.notifyAlerts)
    val advancedHeader: View = a.findViewById(R.id.advancedHeader)
    val advancedChevron: ImageView = a.findViewById(R.id.advancedChevron)
    val advancedBody: View = a.findViewById(R.id.advancedBody)
    val demo: MaterialSwitch = a.findViewById(R.id.demo)
    val sample: MaterialSwitch = a.findViewById(R.id.sample)
    val origin: TextInputEditText = a.findViewById(R.id.origin)
    val linkGroup: MaterialButtonToggleGroup = a.findViewById(R.id.linkGroup)
    val linkBle: MaterialButton = a.findViewById(R.id.linkBle)
    val hostPort: TextInputEditText = a.findViewById(R.id.hostPort)
    val speedup: TextInputEditText = a.findViewById(R.id.speedup)
    val plan: MaterialButton = a.findViewById(R.id.btnPlan)
    val mapNote: TextView = a.findViewById(R.id.mapNote)
    val status: TextView = a.findViewById(R.id.status)
    val log: TextView = a.findViewById(R.id.log)
    val start: MaterialButton = a.findViewById(R.id.btnStart)
    val usageBody: TextView = a.findViewById(R.id.usageBody)
    val usageNote: TextView = a.findViewById(R.id.usageNote)
    val usageCard: MaterialCardView = a.findViewById(R.id.usageCard)
}
