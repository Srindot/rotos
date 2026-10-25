package dev.rotos.app.ui

import android.content.Context
import android.content.res.Configuration
import android.os.Bundle
import androidx.core.content.ContextCompat
import com.google.android.gms.maps.CameraUpdateFactory
import com.google.android.gms.maps.GoogleMap
import com.google.android.gms.maps.MapView
import com.google.android.gms.maps.model.BitmapDescriptorFactory
import com.google.android.gms.maps.model.JointType
import com.google.android.gms.maps.model.LatLng
import com.google.android.gms.maps.model.LatLngBounds
import com.google.android.gms.maps.model.MapStyleOptions
import com.google.android.gms.maps.model.MarkerOptions
import com.google.android.gms.maps.model.PolylineOptions
import com.google.android.gms.maps.model.RoundCap
import dev.rotos.app.R
import dev.rotos.app.route.LatLon

/**
 * The Google map with the planned route (only used when an API key is
 * configured): route in mauve over a darker casing, start / end markers, and
 * the Catppuccin Mocha map style when the phone is in dark mode.
 */
class RouteMap(private val view: MapView, private val context: Context) {
    private var map: GoogleMap? = null
    private var pending: List<LatLon> = emptyList()
    private var pendingLine = true

    fun onCreate(state: Bundle?) {
        view.onCreate(state)
        view.getMapAsync { m ->
            map = m
            m.uiSettings.isMapToolbarEnabled = false
            m.uiSettings.isCompassEnabled = false
            if (isNight()) runCatching { m.setMapStyle(MapStyleOptions.loadRawResourceStyle(context, R.raw.map_style_mocha)) }
            show(pending, pendingLine)
        }
    }

    /**
     * The planned route ([line]) or, for an area map, just the area: camera
     * on its bounds and a marker where the rider is.
     */
    fun show(route: List<LatLon>, line: Boolean = true) {
        pending = route
        pendingLine = line
        val m = map ?: return
        m.clear()
        if (route.size < 2) return
        val ll = route.map { LatLng(it.lat, it.lon) }
        if (!line) {                                     // area maps are centred on the rider
            val centre = LatLngBounds.builder().apply { ll.forEach { include(it) } }.build().center
            m.addMarker(MarkerOptions().position(centre)
                .icon(BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_AZURE)))
            frame(m, ll)
            return
        }
        m.addPolyline(PolylineOptions().addAll(ll).color(color(R.color.ctp_crust)).width(18f)
            .jointType(JointType.ROUND).startCap(RoundCap()).endCap(RoundCap()))
        m.addPolyline(PolylineOptions().addAll(ll).color(color(R.color.ctp_mauve)).width(11f)
            .jointType(JointType.ROUND).startCap(RoundCap()).endCap(RoundCap()))
        m.addMarker(MarkerOptions().position(ll.first())
            .icon(BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_AZURE)))
        m.addMarker(MarkerOptions().position(ll.last())
            .icon(BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_VIOLET)))
        frame(m, ll)
    }

    private fun frame(m: GoogleMap, ll: List<LatLng>) {
        val b = LatLngBounds.builder()
        ll.forEach { b.include(it) }
        view.post { runCatching { m.moveCamera(CameraUpdateFactory.newLatLngBounds(b.build(), 64)) } }
    }

    private fun color(id: Int): Int = ContextCompat.getColor(context, id)

    private fun isNight(): Boolean =
        (context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES

    fun onStart() = view.onStart()
    fun onResume() = view.onResume()
    fun onPause() = view.onPause()
    fun onStop() = view.onStop()
    fun onDestroy() = view.onDestroy()
    fun onLowMemory() = view.onLowMemory()
    fun onSaveInstanceState(out: Bundle) = view.onSaveInstanceState(out)
}
