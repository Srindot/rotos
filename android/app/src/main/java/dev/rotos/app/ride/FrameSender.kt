package dev.rotos.app.ride

import android.os.SystemClock
import dev.rotos.app.call.CallFrame
import dev.rotos.app.link.RotosLink
import dev.rotos.app.link.SeqCounter
import dev.rotos.app.location.Fix
import dev.rotos.app.mirror.Guidance
import dev.rotos.app.notify.PhoneNote
import dev.rotos.app.nav.FrameCodec
import dev.rotos.app.nav.NativeNav
import dev.rotos.app.nav.NavThread
import dev.rotos.app.nav.Packets
import dev.rotos.app.phone.Forecast
import dev.rotos.app.phone.PhoneStatus
import dev.rotos.app.phone.Weather
import dev.rotos.app.phone.WeatherClient
import kotlinx.coroutines.sync.withLock

/**
 * Every frame the phone sends to ROTOS, as one typed call each. Frames are
 * built by the C library (NativeNav, on NavThread) and go out on [link];
 * one u16 [seq] counter numbers them all, the map transfer's included.
 * A failed send is dropped: the next periodic frame carries fresh data.
 */
class FrameSender(val link: RotosLink) {
    val seq = SeqCounter()

    /** When the last frame left (the dashboard pings only when idle). */
    @Volatile var lastSentMs = 0L
        private set

    fun markSent() {
        lastSentMs = System.currentTimeMillis()
    }

    suspend fun prep(stage: Int, pct: Int = 0, done: Int = 0, total: Int = 0) =
        frame { NativeNav.framePrepStatus(seq.next(), tMs(), stage, pct, done, total, it) }

    suspend fun guidance(g: Guidance) {
        val flags = Packets.GUIDE_ACTIVE or if (g.arrived) Packets.GUIDE_ARRIVED else 0
        val toTurn = g.toTurnM?.coerceIn(0, 0xFFFE) ?: 0xFFFF
        val eta = g.eta?.toSecondOfDay()?.toLong() ?: 0xFFFFFFFFL
        direct { FrameCodec.guidance(it, tMs(), g.maneuver, flags, toTurn, (g.remainingM ?: -1).toLong() and 0xFFFFFFFFL, eta, g.road) }
    }

    /** Google Maps stopped navigating: ROTOS leaves its turn screen. */
    suspend fun guidanceOff() = direct { FrameCodec.guidance(it, tMs(), 0, 0, 0xFFFF, 0xFFFFFFFFL, 0xFFFFFFFFL, "") }

    suspend fun ping() = frame { NativeNav.framePing(seq.next(), it) }

    suspend fun phoneStatus(s: PhoneStatus, flags: Int) = frame {
        NativeNav.framePhoneStatus(seq.next(), tMs(), s.timeOfDayS, s.weekday, s.day, s.month, s.batteryPct, flags,
            s.signalBars, it)
    }

    suspend fun weather(x: Weather) = frame {
        NativeNav.frameWeather(seq.next(), tMs(), x.tempC, x.code, x.rainPct, x.windKmh, if (x.isDay) 1 else 0, it)
    }

    suspend fun forecast(f: Forecast) {
        val n = minOf(f.hours.size, WeatherClient.HOURS)
        val temps = FloatArray(n) { f.hours[it].tempC }
        val hourly = IntArray(3 * n) { k ->
            f.hours[k / 3].let { h -> when (k % 3) { 0 -> h.code; 1 -> h.rainPct; else -> h.windKmh } }
        }
        frame { NativeNav.frameForecast(seq.next(), tMs(), f.startHour, n, f.sunriseMin, f.sunsetMin, temps, hourly, it) }
    }

    suspend fun notify(n: PhoneNote) = frame {
        NativeNav.frameNotify(seq.next(), tMs(), n.id, if (n.message) Packets.NOTIFY_MESSAGE else Packets.NOTIFY_OTHER,
            n.app, n.title, n.text, it)
    }

    suspend fun callState(f: CallFrame) =
        frame { NativeNav.frameCallState(seq.next(), tMs(), f.callId, f.state, f.flags, f.name, it) }

    /**
     * NAV_STATE in free mode: the rider's position, plus where they are on the
     * line if [along] is known. Encoded in Kotlin (FrameCodec): it never waits
     * for NavThread, even while a map is being built.
     */
    suspend fun position(fix: Fix, routeId: Int, flags: Int, along: Along?, remainingM: Float, etaS: Int) = direct {
        FrameCodec.position(it, tMs(), routeId, fix.lat, fix.lon, maxOf(fix.speedMps, 0f),
            if (fix.headingDeg >= 0f) fix.headingDeg else 0f, fix.accuracyM, along?.alongM ?: 0f, remainingM, etaS, flags)
    }

    /** A whole route line as ROUTE chunks, in one burst (~14 KB at most). */
    suspend fun routeLine(line: RouteLine) = seq.order.withLock {
        val first = seq.next()
        val frames = FrameCodec.routeChunks(first, line.id, line.packRouteId, line.xs, line.ys, line.along)
        repeat(frames.size - 1) { seq.next() }
        val burst = java.io.ByteArrayOutputStream(frames.sumOf { it.size })
        frames.forEach { burst.write(it) }
        link.send(burst.toByteArray()).onSuccess { markSent() }
    }

    /**
     * A frame built elsewhere (the engine's NAV_STATE) from the sequence
     * number it is given; null = nothing to send.
     */
    suspend fun numbered(build: suspend (Int) -> ByteArray?) = seq.order.withLock {
        build(seq.next())?.let { bytes -> link.send(bytes).onSuccess { markSent() } }
    }

    /** A frame built in Kotlin from its sequence number (no NavThread). */
    private suspend fun direct(build: (Int) -> ByteArray) = seq.order.withLock {
        link.send(build(seq.next())).onSuccess { markSent() }
    }

    private suspend fun frame(build: (ByteArray) -> Int) = seq.order.withLock {
        val buf = ByteArray(NativeNav.MAX_FRAME)
        val n = NavThread.run { build(buf) }
        if (n > 0) link.send(buf.copyOf(n)).onSuccess { markSent() }
    }

    companion object {
        /** Milliseconds since boot, the `t_ms` of every frame. */
        fun tMs(): Int = SystemClock.elapsedRealtime().toInt()
    }
}
