package dev.rotos.app.nav

/**
 * JNI view of the shared C navigation library (navigation/include/rn/rn_ffi.h),
 * implemented by app/src/main/cpp/rotos_jni.c.
 *
 * The library holds ONE static navigation session, so every call must run on
 * [NavThread]. Results follow rn_ffi.h: >= 0 success, negative rn_status.
 * Unsigned C values (u32 CRCs, route ids, times) travel as the same bits in Int.
 */
object NativeNav {
    init {
        System.loadLibrary("rotos_jni")
    }

    const val ERR_PARAM = -1
    const val ERR_STATE = -6
    const val MAX_FRAME = 258
    const val MAX_PAYLOAD = 248
    const val ROAD_BYTES = 41

    external fun apiVersion(): Int
    external fun reset()
    external fun assertCount(): Int

    // ---- route
    external fun routeAddPolyline(encoded: String): Int
    external fun routeAddPoint(lat: Double, lon: Double): Int
    external fun routePointCount(): Int
    external fun routeAddManeuver(type: Int, pointIdx: Int, road: String): Int
    external fun routeAddManeuverGoogle(maneuver: String, pointIdx: Int, instruction: String): Int
    external fun routeFinish(durationS: Float, routeId: Int): Int
    external fun routeTotalM(): Float
    external fun routeDurationS(): Float
    external fun routeManeuverCount(): Int
    /** info = [type, hint, along_m]; road >= [ROAD_BYTES], NUL-terminated ASCII. */
    external fun routeManeuver(i: Int, info: FloatArray, road: ByteArray): Int
    /** out = [lat, lon, along_m] */
    external fun routePoint(i: Int, out: DoubleArray): Int

    // ---- flyover hints
    external fun bridgesClear()
    external fun bridgesAdd(lat: DoubleArray, lon: DoubleArray, n: Int): Int
    external fun bridgesCount(): Int
    external fun flyoverAnnotate(): Int
    external fun flyoverByName(): Int

    // ---- corridor, map generation, map pack
    external fun corridorPlan(zoom: Int, bufferPx: Int): Int
    /** out = [origin_x, origin_y, tiles_w, tiles_h, n_keep] */
    external fun corridorInfo(out: IntArray): Int
    external fun corridorKeep(cx: Int, cy: Int): Int
    /** out = [x, y]: route vertex i in pack pixels (relative to the corridor origin). */
    external fun corridorPointPx(i: Int, out: FloatArray): Int
    external fun mapgenBlock(rgb: ByteArray, w: Int, h: Int, out: ByteArray): Int
    /** Tile style of the next blocks: 0 OSM carto, 1 ROTOS's Google style (keeps narrow lanes). */
    external fun mapgenSource(source: Int): Int
    external fun packBegin(): Int
    external fun packAddTile(cx: Int, cy: Int, classes: ByteArray): Int
    external fun packFinish(): Int
    external fun packCopy(out: ByteArray): Int
    /** Raw deflate, 4 KB window (ROTOS's decoder): src[0..n) into dst; compressed length or < 0. */
    external fun deflateRaw(src: ByteArray, n: Int, dst: ByteArray): Int
    /** The current pack compressed for the transfer; the map frames then carry it. */
    external fun wireSet(z: ByteArray, n: Int): Int
    /** Bytes the map transfer sends (compressed or raw). */
    external fun wireLen(): Int
    /** The current pack as a patch against [base] (the pack ROTOS holds); patch length or < 0. */
    external fun delta(base: ByteArray, n: Int, out: ByteArray): Int
    /** A deflated patch as the transfer's bytes; MAP_BEGIN then names [baseCrc]. */
    external fun wireSetDelta(z: ByteArray, n: Int, baseCrc: Int): Int
    external fun packCrc32(): Int
    external fun packRouteId(): Int
    external fun packLoad(blob: ByteArray, len: Int): Int
    external fun packClassAt(x: Int, y: Int): Int
    /** Classes of pack pixels (x0 + i*step, y), i < n; -1 outside the corridor. */
    external fun packClassRow(x0: Int, y: Int, step: Int, out: ByteArray, n: Int): Int

    // ---- navigation engine
    external fun engineReset()
    external fun engineFix(
        seq: Int, tMs: Int, unixS: Double, utcOffsetS: Int, lat: Double, lon: Double,
        speedMps: Float, headingDeg: Float, accuracyM: Float, out: ByteArray,
    ): Int
    /** f = [along_m, remaining_m]; i = [remaining_s, flags, wants_reroute] */
    external fun engineState(f: FloatArray, i: IntArray): Int

    // ---- outbound frames
    external fun framePhoneStatus(
        seq: Int, tMs: Int, timeS: Int, weekday: Int, day: Int, month: Int,
        batteryPct: Int, flags: Int, signalBars: Int, out: ByteArray,
    ): Int
    /** NAV_STATE with RN_NAV_FREE: the rider's GPS position for ROTOS's map (Google Maps navigates). */
    external fun framePosition(
        seq: Int, tMs: Int, routeId: Int, lat: Double, lon: Double, speedMps: Float, headingDeg: Float,
        accuracyM: Float, alongM: Float, remainingM: Float, etaS: Int, flags: Int, out: ByteArray,
    ): Int
    /** stage = RN_PREP_* (Packets.PREP_*), pct 0..100 for the whole job, done/total in stage units. */
    external fun framePrepStatus(seq: Int, tMs: Int, stage: Int, pct: Int, done: Int, total: Int, out: ByteArray): Int
    /** GUIDANCE (mirror mode). Unknown to_turn = 0xFFFF, unknown remaining / eta = -1 (all ones). */
    external fun frameGuidance(
        seq: Int, tMs: Int, maneuver: Int, flags: Int, toTurnM: Int, remainingM: Int, etaS: Int, road: String,
        out: ByteArray,
    ): Int
    external fun frameWeather(
        seq: Int, tMs: Int, tempC: Float, code: Int, rainProb: Int, windKmh: Int,
        isDay: Int, out: ByteArray,
    ): Int
    external fun frameSettings(
        seq: Int, themeMode: Int, northUp: Int, glassPct: Int, brightnessPct: Int, out: ByteArray,
    ): Int
    external fun framePing(seq: Int, out: ByteArray): Int
    /** tempC[n]; hourly[3n] = (WMO code, rain %, wind km/h) per hour; n <= 8. */
    external fun frameForecast(
        seq: Int, tMs: Int, startHour: Int, n: Int, sunriseMin: Int, sunsetMin: Int,
        tempC: FloatArray, hourly: IntArray, out: ByteArray,
    ): Int
    external fun frameCallState(seq: Int, tMs: Int, callId: Int, state: Int, flags: Int, name: String, out: ByteArray): Int
    /** NOTIFY: a phone notification for ROTOS (kind: Packets.NOTIFY_MESSAGE / NOTIFY_OTHER). */
    external fun frameNotify(seq: Int, tMs: Int, id: Int, kind: Int, app: String, title: String, text: String,
                             out: ByteArray): Int
    external fun frameMapBegin(seq: Int, out: ByteArray): Int
    external fun frameMapChunk(seq: Int, offset: Int, out: ByteArray): Int
    external fun frameMapEnd(seq: Int, out: ByteArray): Int
    external fun mapChunkSize(): Int

    // ---- inbound frames
    external fun rxReset()
    external fun rxFeed(bytes: ByteArray, n: Int): Int
    /** hdr = [type, seq]; payload >= [MAX_PAYLOAD]. Returns length, -1 when empty. */
    external fun rxNext(hdr: IntArray, payload: ByteArray): Int
    /** out = [acked_type, status, acked_seq, value] */
    external fun decodeAck(payload: ByteArray, len: Int, out: IntArray): Int
    /** out = [button, event] */
    external fun decodeButton(payload: ByteArray, len: Int, out: IntArray): Int
    /** out = [call_id, action] */
    external fun decodeCallAction(payload: ByteArray, len: Int, out: IntArray): Int

    // ---- ride simulator
    external fun simStart(seed: Int, cruiseMps: Float): Int
    external fun simStep(dt: Float): Int
    /** ll = [lat, lon]; f = [speed_mps, heading_deg, accuracy_m] */
    external fun simFix(sigmaM: Float, ll: DoubleArray, f: FloatArray): Int
    external fun simAlong(): Float
    external fun simSpeed(): Float
    external fun simTime(): Float

    // ---- misc
    external fun sunElevation(lat: Double, lon: Double, unixS: Double): Double
    external fun localSeconds(unixS: Double, utcOffsetS: Int): Int
}
