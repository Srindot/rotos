/*
 * rotos_jni.c - thin JNI bridge from dev.rotos.app.nav.NativeNav (Kotlin) to
 * the flat navigation API in rn/rn_ffi.h.
 *
 * Rules (docs/CODING_STANDARD.md): every parameter is validated, every array
 * obtained from the VM is released on every path, no allocation except the
 * VM's own array / string access, no function pointers (methods are bound by
 * their JNI names, not RegisterNatives). Unsigned C values travel as the
 * same bit pattern in Java ints. Results follow rn_ffi.h: >= 0 success,
 * negative rn_status on failure (RN_ERR_PARAM for a bad Java argument).
 *
 * One exception: deflateRaw lets zlib allocate its own small working state
 * (deflateInit2 / deflateEnd), freed before it returns.
 *
 * All calls must come from one thread at a time (NavThread in Kotlin): the
 * library keeps one static navigation session.
 */
#include <jni.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>

#include "rn/rn_ffi.h"
#include "rn/rn_protocol.h"
#include "rn/rn_text.h"

#define JNI_ROAD_BYTES  (RN_ROAD_NAME_MAX + 1u)

/* ---- helpers ------------------------------------------------------------- */

/* Elements of a byte array with at least `need` bytes, or NULL. */
static jbyte *bytes_get(JNIEnv *env, jbyteArray a, jsize need)
{
    if (a == NULL || need < 0 || (*env)->GetArrayLength(env, a) < need) {
        return NULL;
    }
    return (*env)->GetByteArrayElements(env, a, NULL);
}

/* Copies a frame built on the stack into a Java array; returns n or an error. */
static jint frame_out(JNIEnv *env, jbyteArray out, const uint8_t *buf, int32_t n)
{
    if (n <= 0) {
        return n;
    }
    if (out == NULL || (*env)->GetArrayLength(env, out) < n) {
        return RN_ERR_FULL;
    }
    (*env)->SetByteArrayRegion(env, out, 0, n, (const jbyte *)buf);
    return n;
}

static jboolean has_len(JNIEnv *env, jarray a, jsize need)
{
    return (a != NULL && (*env)->GetArrayLength(env, a) >= need) ? JNI_TRUE : JNI_FALSE;
}

/* ---- session ------------------------------------------------------------- */

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_apiVersion(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_api_version();
}

JNIEXPORT void JNICALL Java_dev_rotos_app_nav_NativeNav_reset(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    rn_ffi_reset();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_assertCount(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return (jint)rn_ffi_assert_count();
}

/* ---- route --------------------------------------------------------------- */

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routeAddPolyline(JNIEnv *env, jobject self,
                                                                          jstring encoded)
{
    (void)self;
    if (encoded == NULL) {
        return RN_ERR_PARAM;
    }
    const char *s = (*env)->GetStringUTFChars(env, encoded, NULL);
    if (s == NULL) {
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_route_add_polyline(s);
    (*env)->ReleaseStringUTFChars(env, encoded, s);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routeAddPoint(JNIEnv *env, jobject self,
                                                                       jdouble lat, jdouble lon)
{
    (void)env;
    (void)self;
    return rn_ffi_route_add_point(lat, lon);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routePointCount(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_route_point_count();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routeAddManeuver(JNIEnv *env, jobject self,
                                                                          jint type, jint idx,
                                                                          jstring road)
{
    (void)self;
    if (road == NULL || type < 0 || idx < 0) {
        return RN_ERR_PARAM;
    }
    const char *s = (*env)->GetStringUTFChars(env, road, NULL);
    if (s == NULL) {
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_route_add_maneuver(type, idx, s);
    (*env)->ReleaseStringUTFChars(env, road, s);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routeAddManeuverGoogle(
    JNIEnv *env, jobject self, jstring maneuver, jint idx, jstring instruction)
{
    (void)self;
    if (maneuver == NULL || instruction == NULL || idx < 0) {
        return RN_ERR_PARAM;
    }
    const char *m = (*env)->GetStringUTFChars(env, maneuver, NULL);
    if (m == NULL) {
        return RN_ERR_PARAM;
    }
    const char *t = (*env)->GetStringUTFChars(env, instruction, NULL);
    if (t == NULL) {
        (*env)->ReleaseStringUTFChars(env, maneuver, m);
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_route_add_maneuver_google(m, idx, t);
    (*env)->ReleaseStringUTFChars(env, instruction, t);
    (*env)->ReleaseStringUTFChars(env, maneuver, m);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routeFinish(JNIEnv *env, jobject self,
                                                                     jfloat duration_s, jint route_id)
{
    (void)env;
    (void)self;
    if (duration_s < 0.0f) {
        return RN_ERR_PARAM;
    }
    return rn_ffi_route_finish(duration_s, (uint32_t)route_id);
}

JNIEXPORT jfloat JNICALL Java_dev_rotos_app_nav_NativeNav_routeTotalM(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_route_total_m();
}

JNIEXPORT jfloat JNICALL Java_dev_rotos_app_nav_NativeNav_routeDurationS(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_route_duration_s();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routeManeuverCount(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_route_maneuver_count();
}

/* info[3] = type, hint, along_m; road: >= RN_ROAD_NAME_MAX + 1 bytes, NUL-terminated. */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routeManeuver(JNIEnv *env, jobject self,
                                                                       jint i, jfloatArray info,
                                                                       jbyteArray road)
{
    (void)self;
    if (!has_len(env, info, 3) || !has_len(env, road, (jsize)JNI_ROAD_BYTES)) {
        return RN_ERR_PARAM;
    }
    uint8_t type = 0u;
    uint8_t hint = 0u;
    float along = 0.0f;
    char name[JNI_ROAD_BYTES] = {0};
    int32_t r = rn_ffi_route_maneuver(i, &type, &hint, &along, name);
    if (r < 0) {
        return r;
    }
    jfloat v[3] = {(jfloat)type, (jfloat)hint, along};
    (*env)->SetFloatArrayRegion(env, info, 0, 3, v);
    (*env)->SetByteArrayRegion(env, road, 0, (jsize)JNI_ROAD_BYTES, (const jbyte *)name);
    return r;
}

/* out[3] = lat, lon, along_m */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_routePoint(JNIEnv *env, jobject self,
                                                                    jint i, jdoubleArray out)
{
    (void)self;
    if (!has_len(env, out, 3)) {
        return RN_ERR_PARAM;
    }
    double lat = 0.0;
    double lon = 0.0;
    float along = 0.0f;
    int32_t r = rn_ffi_route_point(i, &lat, &lon, &along);
    if (r < 0) {
        return r;
    }
    jdouble v[3] = {lat, lon, (jdouble)along};
    (*env)->SetDoubleArrayRegion(env, out, 0, 3, v);
    return r;
}

/* ---- flyover hints ------------------------------------------------------- */

JNIEXPORT void JNICALL Java_dev_rotos_app_nav_NativeNav_bridgesClear(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    rn_ffi_bridges_clear();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_bridgesAdd(JNIEnv *env, jobject self,
                                                                    jdoubleArray lat,
                                                                    jdoubleArray lon, jint n)
{
    (void)self;
    if (n < 0 || !has_len(env, lat, n) || !has_len(env, lon, n)) {
        return RN_ERR_PARAM;
    }
    jdouble *a = (*env)->GetDoubleArrayElements(env, lat, NULL);
    if (a == NULL) {
        return RN_ERR_PARAM;
    }
    jdouble *b = (*env)->GetDoubleArrayElements(env, lon, NULL);
    if (b == NULL) {
        (*env)->ReleaseDoubleArrayElements(env, lat, a, JNI_ABORT);
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_bridges_add(a, b, n);
    (*env)->ReleaseDoubleArrayElements(env, lon, b, JNI_ABORT);
    (*env)->ReleaseDoubleArrayElements(env, lat, a, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_bridgesCount(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_bridges_count();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_flyoverAnnotate(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_flyover_annotate();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_flyoverByName(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_flyover_by_name();
}

/* ---- corridor, mapgen, pack ---------------------------------------------- */

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_corridorPlan(JNIEnv *env, jobject self,
                                                                      jint zoom, jint buffer_px)
{
    (void)env;
    (void)self;
    return rn_ffi_corridor_plan(zoom, buffer_px);
}

/* out[5] = origin_x, origin_y, tiles_w, tiles_h, n_keep */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_corridorInfo(JNIEnv *env, jobject self,
                                                                      jintArray out)
{
    (void)self;
    if (!has_len(env, out, 5)) {
        return RN_ERR_PARAM;
    }
    jint v[5] = {0, 0, 0, 0, 0};
    int32_t r = rn_ffi_corridor_info(&v[0], &v[1], &v[2], &v[3], &v[4]);
    if (r < 0) {
        return r;
    }
    (*env)->SetIntArrayRegion(env, out, 0, 5, v);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_corridorKeep(JNIEnv *env, jobject self,
                                                                      jint cx, jint cy)
{
    (void)env;
    (void)self;
    return rn_ffi_corridor_keep(cx, cy);
}

/* out[2] = x, y: route vertex i in pack pixels (relative to the corridor origin). */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_corridorPointPx(JNIEnv *env, jobject self,
                                                                         jint i, jfloatArray out)
{
    (void)self;
    if (!has_len(env, out, 2)) {
        return RN_ERR_PARAM;
    }
    float x = 0.0f;
    float y = 0.0f;
    int32_t r = rn_ffi_corridor_point_px(i, &x, &y);
    if (r < 0) {
        return r;
    }
    jfloat v[2] = {x, y};
    (*env)->SetFloatArrayRegion(env, out, 0, 2, v);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_mapgenBlock(JNIEnv *env, jobject self,
                                                                     jbyteArray rgb, jint w, jint h,
                                                                     jbyteArray out)
{
    (void)self;
    if (w <= 0 || h <= 0 || w > 576 || h > 576 || (w % 2) != 0 || (h % 2) != 0) {
        return RN_ERR_PARAM;
    }
    jbyte *in = bytes_get(env, rgb, w * h * 3);
    if (in == NULL) {
        return RN_ERR_PARAM;
    }
    jbyte *cls = bytes_get(env, out, (w / 2) * (h / 2));
    if (cls == NULL) {
        (*env)->ReleaseByteArrayElements(env, rgb, in, JNI_ABORT);
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_mapgen_block((const uint8_t *)in, w, h, (uint8_t *)cls);
    (*env)->ReleaseByteArrayElements(env, out, cls, 0);
    (*env)->ReleaseByteArrayElements(env, rgb, in, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packBegin(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_pack_begin();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packAddTile(JNIEnv *env, jobject self,
                                                                     jint cx, jint cy,
                                                                     jbyteArray classes)
{
    (void)self;
    jbyte *c = bytes_get(env, classes, 32 * 32);
    if (c == NULL) {
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_pack_add_tile(cx, cy, (const uint8_t *)c);
    (*env)->ReleaseByteArrayElements(env, classes, c, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packFinish(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_pack_finish();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packCopy(JNIEnv *env, jobject self,
                                                                  jbyteArray out)
{
    (void)self;
    if (out == NULL) {
        return RN_ERR_PARAM;
    }
    jsize cap = (*env)->GetArrayLength(env, out);
    jbyte *b = (*env)->GetByteArrayElements(env, out, NULL);
    if (b == NULL) {
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_pack_copy((uint8_t *)b, cap);
    (*env)->ReleaseByteArrayElements(env, out, b, (r >= 0) ? 0 : JNI_ABORT);
    return r;
}

/* Raw deflate with a 4 KB window (what ROTOS's rn_inflate holds): src[0..n) -> dst. */
static int32_t deflate_raw(const uint8_t *src, uint32_t n, uint8_t *dst, uint32_t cap)
{
    z_stream s;
    (void)memset(&s, 0, sizeof s);
    if (deflateInit2(&s, 9, Z_DEFLATED, -12, 9, Z_DEFAULT_STRATEGY) != Z_OK) {
        return RN_ERR_STATE;
    }
    s.next_in = (Bytef *)src;
    s.avail_in = n;
    s.next_out = dst;
    s.avail_out = cap;
    int rc = deflate(&s, Z_FINISH);
    uint32_t out = (uint32_t)s.total_out;
    (void)deflateEnd(&s);
    return (rc == Z_STREAM_END) ? (int32_t)out : RN_ERR_FULL;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_deflateRaw(JNIEnv *env, jobject self, jbyteArray src,
                                                                   jint n, jbyteArray dst)
{
    (void)self;
    if (src == NULL || dst == NULL || n < 0) {
        return RN_ERR_PARAM;
    }
    jsize cap = (*env)->GetArrayLength(env, dst);
    jbyte *in = bytes_get(env, src, n);
    if (in == NULL) {
        return RN_ERR_PARAM;
    }
    jbyte *out = (*env)->GetByteArrayElements(env, dst, NULL);
    if (out == NULL) {
        (*env)->ReleaseByteArrayElements(env, src, in, JNI_ABORT);
        return RN_ERR_PARAM;
    }
    int32_t r = deflate_raw((const uint8_t *)in, (uint32_t)n, (uint8_t *)out, (uint32_t)cap);
    (*env)->ReleaseByteArrayElements(env, dst, out, (r >= 0) ? 0 : JNI_ABORT);
    (*env)->ReleaseByteArrayElements(env, src, in, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_wireSet(JNIEnv *env, jobject self, jbyteArray z, jint n)
{
    (void)self;
    jbyte *b = bytes_get(env, z, n);
    if (b == NULL) {
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_wire_set((const uint8_t *)b, n);
    (*env)->ReleaseByteArrayElements(env, z, b, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_mapgenSource(JNIEnv *env, jobject self, jint source)
{
    (void)env;
    (void)self;
    return rn_ffi_mapgen_source(source);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_delta(JNIEnv *env, jobject self, jbyteArray base, jint n,
                                                              jbyteArray out)
{
    (void)self;
    if (out == NULL) {
        return RN_ERR_PARAM;
    }
    jbyte *b = bytes_get(env, base, n);
    if (b == NULL) {
        return RN_ERR_PARAM;
    }
    jsize cap = (*env)->GetArrayLength(env, out);
    jbyte *o = (*env)->GetByteArrayElements(env, out, NULL);
    if (o == NULL) {
        (*env)->ReleaseByteArrayElements(env, base, b, JNI_ABORT);
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_delta((const uint8_t *)b, n, (uint8_t *)o, cap);
    (*env)->ReleaseByteArrayElements(env, out, o, (r >= 0) ? 0 : JNI_ABORT);
    (*env)->ReleaseByteArrayElements(env, base, b, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_wireSetDelta(JNIEnv *env, jobject self, jbyteArray z, jint n,
                                                                     jint base_crc)
{
    (void)self;
    jbyte *b = bytes_get(env, z, n);
    if (b == NULL) {
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_wire_set_delta((const uint8_t *)b, n, (uint32_t)base_crc);
    (*env)->ReleaseByteArrayElements(env, z, b, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_wireLen(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_wire_len();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packCrc32(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return (jint)rn_ffi_pack_crc32();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packRouteId(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return (jint)rn_ffi_pack_route_id();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packLoad(JNIEnv *env, jobject self,
                                                                  jbyteArray blob, jint len)
{
    (void)self;
    if (len <= 0) {
        return RN_ERR_PARAM;
    }
    jbyte *b = bytes_get(env, blob, len);
    if (b == NULL) {
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_pack_load((const uint8_t *)b, len);
    (*env)->ReleaseByteArrayElements(env, blob, b, JNI_ABORT);
    return r;
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packClassAt(JNIEnv *env, jobject self,
                                                                     jint x, jint y)
{
    (void)env;
    (void)self;
    return rn_ffi_pack_class_at(x, y);
}

/* Fills a whole row of classes (preview rendering without one JNI call per pixel). */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_packClassRow(JNIEnv *env, jobject self,
                                                                      jint x0, jint y, jint step,
                                                                      jbyteArray out, jint n)
{
    (void)self;
    if (n <= 0 || n > 4096 || step <= 0 || !has_len(env, out, n)) {
        return RN_ERR_PARAM;
    }
    jbyte row[4096];
    for (jint i = 0; i < n && i < 4096; i++) {
        row[i] = (jbyte)rn_ffi_pack_class_at(x0 + i * step, y);
    }
    (*env)->SetByteArrayRegion(env, out, 0, n, row);
    return n;
}

/* ---- navigation engine --------------------------------------------------- */

JNIEXPORT void JNICALL Java_dev_rotos_app_nav_NativeNav_engineReset(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    rn_ffi_engine_reset();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_engineFix(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jdouble unix_s, jint utc_offset_s, jdouble lat,
    jdouble lon, jfloat speed_mps, jfloat heading_deg, jfloat accuracy_m, jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_engine_fix((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms, unix_s, utc_offset_s,
                                  lat, lon, speed_mps, heading_deg, accuracy_m, buf,
                                  (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

/* f[2] = along_m, remaining_m; i[3] = remaining_s, flags, wants_reroute */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_engineState(JNIEnv *env, jobject self,
                                                                     jfloatArray f, jintArray i)
{
    (void)self;
    if (!has_len(env, f, 2) || !has_len(env, i, 3)) {
        return RN_ERR_PARAM;
    }
    float along = 0.0f;
    float remaining = 0.0f;
    uint32_t rem_s = 0u;
    uint8_t flags = 0u;
    int32_t wants = 0;
    int32_t r = rn_ffi_engine_state(&along, &remaining, &rem_s, &flags, &wants);
    if (r < 0) {
        return r;
    }
    jfloat fv[2] = {along, remaining};
    jint iv[3] = {(jint)rem_s, (jint)flags, wants};
    (*env)->SetFloatArrayRegion(env, f, 0, 2, fv);
    (*env)->SetIntArrayRegion(env, i, 0, 3, iv);
    return r;
}

/* ---- outbound frames ----------------------------------------------------- */

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_framePhoneStatus(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jint time_s, jint weekday, jint day, jint month,
    jint battery_pct, jint flags, jint signal_bars, jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_phone_status((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms,
                                          (uint32_t)time_s, (uint8_t)(weekday & 0xFF),
                                          (uint8_t)(day & 0xFF), (uint8_t)(month & 0xFF),
                                          (uint8_t)(battery_pct & 0xFF), (uint8_t)(flags & 0xFF),
                                          (uint8_t)(signal_bars & 0xFF), buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_framePrepStatus(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jint stage, jint pct, jint done, jint total,
    jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_prep_status((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms,
                                         (uint8_t)(stage & 0xFF), (uint8_t)(pct & 0xFF),
                                         (uint16_t)(done & 0xFFFF), (uint16_t)(total & 0xFFFF),
                                         buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameGuidance(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jint maneuver, jint flags, jint to_turn_m,
    jint remaining_m, jint eta_s, jstring road, jbyteArray out)
{
    (void)self;
    char r[64] = "";
    if (road != NULL) {
        const char *s = (*env)->GetStringUTFChars(env, road, NULL);
        if (s != NULL) {
            (void)rn_text_copy(r, (uint32_t)sizeof r, s);
            (*env)->ReleaseStringUTFChars(env, road, s);
        }
    }
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_guidance((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms, (uint8_t)(maneuver & 0xFF),
                                      (uint8_t)(flags & 0xFF), (uint16_t)(to_turn_m & 0xFFFF),
                                      (uint32_t)remaining_m, (uint32_t)eta_s, r, buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_framePosition(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jint route_id, jdouble lat, jdouble lon, jfloat speed_mps,
    jfloat heading_deg, jfloat accuracy_m, jfloat along_m, jfloat remaining_m, jint eta_s, jint flags,
    jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_position((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms, (uint32_t)route_id, lat, lon,
                                      speed_mps, heading_deg, accuracy_m, along_m, remaining_m, (uint32_t)eta_s,
                                      (uint8_t)(flags & 0xFF), buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameWeather(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jfloat temp_c, jint code, jint rain_prob,
    jint wind_kmh, jint is_day, jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_weather((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms, temp_c,
                                     (uint8_t)(code & 0xFF), (uint8_t)(rain_prob & 0xFF),
                                     (uint8_t)(wind_kmh & 0xFF), (uint8_t)(is_day & 0xFF), buf,
                                     (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameSettings(
    JNIEnv *env, jobject self, jint seq, jint theme_mode, jint north_up, jint glass_pct,
    jint brightness_pct, jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_settings((uint16_t)(seq & 0xFFFF), (uint8_t)(theme_mode & 0xFF),
                                      (uint8_t)(north_up & 0xFF), (uint8_t)(glass_pct & 0xFF),
                                      (uint8_t)(brightness_pct & 0xFF), buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

/* temp_c[n] float; hourly[3 * n] = (code, rain %, wind km/h) per hour; n <= 8. */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameForecast(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jint start_hour, jint n, jint sunrise_min,
    jint sunset_min, jfloatArray temp_c, jintArray hourly, jbyteArray out)
{
    (void)self;
    if (n < 0 || n > (jint)RN_FORECAST_HOURS || !has_len(env, temp_c, n) || !has_len(env, hourly, 3 * n) ||
        start_hour < 0 || sunrise_min < 0 || sunset_min < 0) {
        return RN_ERR_PARAM;
    }
    jfloat t[RN_FORECAST_HOURS];
    jint h[3u * RN_FORECAST_HOURS];
    (*env)->GetFloatArrayRegion(env, temp_c, 0, n, t);
    (*env)->GetIntArrayRegion(env, hourly, 0, 3 * n, h);
    uint8_t code[RN_FORECAST_HOURS];
    uint8_t rain[RN_FORECAST_HOURS];
    uint8_t wind[RN_FORECAST_HOURS];
    for (jint i = 0; i < n && i < (jint)RN_FORECAST_HOURS; i++) {
        code[i] = (uint8_t)(h[3 * i] & 0xFF);
        rain[i] = (uint8_t)(h[3 * i + 1] & 0xFF);
        wind[i] = (uint8_t)(h[3 * i + 2] & 0xFF);
    }
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t len = rn_ffi_frame_forecast((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms,
                                        (uint8_t)(start_hour & 0xFF), (uint8_t)n,
                                        (uint16_t)(sunrise_min & 0xFFFF), (uint16_t)(sunset_min & 0xFFFF),
                                        t, code, rain, wind, buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, len);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameCallState(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jint call_id, jint state, jint flags, jstring name,
    jbyteArray out)
{
    (void)self;
    if (name == NULL || state < 0) {
        return RN_ERR_PARAM;
    }
    const char *s = (*env)->GetStringUTFChars(env, name, NULL);
    if (s == NULL) {
        return RN_ERR_PARAM;
    }
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t len = rn_ffi_frame_call_state((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms,
                                          (uint16_t)(call_id & 0xFFFF), (uint8_t)(state & 0xFF),
                                          (uint8_t)(flags & 0xFF), s, buf, (int32_t)sizeof buf);
    (*env)->ReleaseStringUTFChars(env, name, s);
    return frame_out(env, out, buf, len);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameNotify(
    JNIEnv *env, jobject self, jint seq, jint t_ms, jint id, jint kind, jstring app, jstring title, jstring text,
    jbyteArray out)
{
    (void)self;
    if (app == NULL || title == NULL || text == NULL || kind < 0) {
        return RN_ERR_PARAM;
    }
    const char *a = (*env)->GetStringUTFChars(env, app, NULL);
    const char *ti = (*env)->GetStringUTFChars(env, title, NULL);
    const char *tx = (*env)->GetStringUTFChars(env, text, NULL);
    int32_t len = RN_ERR_PARAM;
    uint8_t buf[RN_PKT_MAX_FRAME];
    if (a != NULL && ti != NULL && tx != NULL) {
        len = rn_ffi_frame_notify((uint16_t)(seq & 0xFFFF), (uint32_t)t_ms, (uint16_t)(id & 0xFFFF),
                                  (uint8_t)(kind & 0xFF), a, ti, tx, buf, (int32_t)sizeof buf);
    }
    if (a != NULL) {
        (*env)->ReleaseStringUTFChars(env, app, a);
    }
    if (ti != NULL) {
        (*env)->ReleaseStringUTFChars(env, title, ti);
    }
    if (tx != NULL) {
        (*env)->ReleaseStringUTFChars(env, text, tx);
    }
    return frame_out(env, out, buf, len);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_framePing(JNIEnv *env, jobject self,
                                                                   jint seq, jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_ping((uint16_t)(seq & 0xFFFF), buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameMapBegin(JNIEnv *env, jobject self,
                                                                       jint seq, jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_map_begin((uint16_t)(seq & 0xFFFF), buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameMapChunk(JNIEnv *env, jobject self,
                                                                       jint seq, jint offset,
                                                                       jbyteArray out)
{
    (void)self;
    if (offset < 0) {
        return RN_ERR_PARAM;
    }
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_map_chunk((uint16_t)(seq & 0xFFFF), (uint32_t)offset, buf,
                                       (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_frameMapEnd(JNIEnv *env, jobject self,
                                                                     jint seq, jbyteArray out)
{
    (void)self;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t n = rn_ffi_frame_map_end((uint16_t)(seq & 0xFFFF), buf, (int32_t)sizeof buf);
    return frame_out(env, out, buf, n);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_mapChunkSize(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_map_chunk_size();
}

/* ---- inbound frames ------------------------------------------------------ */

JNIEXPORT void JNICALL Java_dev_rotos_app_nav_NativeNav_rxReset(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    rn_ffi_rx_reset();
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_rxFeed(JNIEnv *env, jobject self,
                                                                jbyteArray bytes, jint n)
{
    (void)self;
    if (n < 0) {
        return RN_ERR_PARAM;
    }
    if (n == 0) {
        return 0;
    }
    jbyte *b = bytes_get(env, bytes, n);
    if (b == NULL) {
        return RN_ERR_PARAM;
    }
    int32_t r = rn_ffi_rx_feed((const uint8_t *)b, n);
    (*env)->ReleaseByteArrayElements(env, bytes, b, JNI_ABORT);
    return r;
}

/* hdr[2] = type, seq. Returns the payload length or -1 when the queue is empty. */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_rxNext(JNIEnv *env, jobject self,
                                                                jintArray hdr, jbyteArray payload)
{
    (void)self;
    if (!has_len(env, hdr, 2) || !has_len(env, payload, (jsize)RN_PKT_MAX_PAYLOAD)) {
        return RN_ERR_PARAM;
    }
    uint8_t type = 0u;
    uint16_t seq = 0u;
    uint8_t buf[RN_PKT_MAX_PAYLOAD];
    int32_t n = rn_ffi_rx_next(&type, &seq, buf, (int32_t)sizeof buf);
    if (n < 0) {
        return n;
    }
    jint hv[2] = {(jint)type, (jint)seq};
    (*env)->SetIntArrayRegion(env, hdr, 0, 2, hv);
    if (n > 0) {
        (*env)->SetByteArrayRegion(env, payload, 0, n, (const jbyte *)buf);
    }
    return n;
}

/* out[4] = acked_type, status, acked_seq, value */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_decodeAck(JNIEnv *env, jobject self,
                                                                   jbyteArray payload, jint len,
                                                                   jintArray out)
{
    (void)self;
    if (len < 0 || !has_len(env, out, 4)) {
        return RN_ERR_PARAM;
    }
    jbyte *p = bytes_get(env, payload, len);
    if (p == NULL) {
        return RN_ERR_PARAM;
    }
    uint8_t type = 0u;
    uint8_t status = 0u;
    uint16_t seq = 0u;
    uint32_t value = 0u;
    int32_t r = rn_ffi_decode_ack((const uint8_t *)p, len, &type, &status, &seq, &value);
    (*env)->ReleaseByteArrayElements(env, payload, p, JNI_ABORT);
    if (r < 0) {
        return r;
    }
    jint v[4] = {(jint)type, (jint)status, (jint)seq, (jint)value};
    (*env)->SetIntArrayRegion(env, out, 0, 4, v);
    return r;
}

/* out[2] = button, event */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_decodeButton(JNIEnv *env, jobject self,
                                                                      jbyteArray payload, jint len,
                                                                      jintArray out)
{
    (void)self;
    if (len < 0 || !has_len(env, out, 2)) {
        return RN_ERR_PARAM;
    }
    jbyte *p = bytes_get(env, payload, len);
    if (p == NULL) {
        return RN_ERR_PARAM;
    }
    uint8_t button = 0u;
    uint8_t event = 0u;
    int32_t r = rn_ffi_decode_button((const uint8_t *)p, len, &button, &event);
    (*env)->ReleaseByteArrayElements(env, payload, p, JNI_ABORT);
    if (r < 0) {
        return r;
    }
    jint v[2] = {(jint)button, (jint)event};
    (*env)->SetIntArrayRegion(env, out, 0, 2, v);
    return r;
}

/* out[2] = call_id, action */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_decodeCallAction(JNIEnv *env, jobject self,
                                                                          jbyteArray payload, jint len,
                                                                          jintArray out)
{
    (void)self;
    if (len < 0 || !has_len(env, out, 2)) {
        return RN_ERR_PARAM;
    }
    jbyte *p = bytes_get(env, payload, len);
    if (p == NULL) {
        return RN_ERR_PARAM;
    }
    uint16_t call_id = 0u;
    uint8_t action = 0u;
    int32_t r = rn_ffi_decode_call_action((const uint8_t *)p, len, &call_id, &action);
    (*env)->ReleaseByteArrayElements(env, payload, p, JNI_ABORT);
    if (r < 0) {
        return r;
    }
    jint v[2] = {(jint)call_id, (jint)action};
    (*env)->SetIntArrayRegion(env, out, 0, 2, v);
    return r;
}

/* ---- ride simulator ------------------------------------------------------ */

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_simStart(JNIEnv *env, jobject self,
                                                                  jint seed, jfloat cruise_mps)
{
    (void)env;
    (void)self;
    return rn_ffi_sim_start((uint32_t)seed, cruise_mps);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_simStep(JNIEnv *env, jobject self,
                                                                 jfloat dt)
{
    (void)env;
    (void)self;
    return rn_ffi_sim_step(dt);
}

/* ll[2] = lat, lon; f[3] = speed_mps, heading_deg, accuracy_m */
JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_simFix(JNIEnv *env, jobject self,
                                                                jfloat sigma_m, jdoubleArray ll,
                                                                jfloatArray f)
{
    (void)self;
    if (!has_len(env, ll, 2) || !has_len(env, f, 3) || sigma_m < 0.0f) {
        return RN_ERR_PARAM;
    }
    double lat = 0.0;
    double lon = 0.0;
    float speed = 0.0f;
    float heading = 0.0f;
    float acc = 0.0f;
    int32_t r = rn_ffi_sim_fix(sigma_m, &lat, &lon, &speed, &heading, &acc);
    if (r < 0) {
        return r;
    }
    jdouble dv[2] = {lat, lon};
    jfloat fv[3] = {speed, heading, acc};
    (*env)->SetDoubleArrayRegion(env, ll, 0, 2, dv);
    (*env)->SetFloatArrayRegion(env, f, 0, 3, fv);
    return r;
}

JNIEXPORT jfloat JNICALL Java_dev_rotos_app_nav_NativeNav_simAlong(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_sim_along();
}

JNIEXPORT jfloat JNICALL Java_dev_rotos_app_nav_NativeNav_simSpeed(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_sim_speed();
}

JNIEXPORT jfloat JNICALL Java_dev_rotos_app_nav_NativeNav_simTime(JNIEnv *env, jobject self)
{
    (void)env;
    (void)self;
    return rn_ffi_sim_time();
}

/* ---- misc ---------------------------------------------------------------- */

JNIEXPORT jdouble JNICALL Java_dev_rotos_app_nav_NativeNav_sunElevation(JNIEnv *env, jobject self,
                                                                         jdouble lat, jdouble lon,
                                                                         jdouble unix_s)
{
    (void)env;
    (void)self;
    return rn_ffi_sun_elevation(lat, lon, unix_s);
}

JNIEXPORT jint JNICALL Java_dev_rotos_app_nav_NativeNav_localSeconds(JNIEnv *env, jobject self,
                                                                      jdouble unix_s,
                                                                      jint utc_offset_s)
{
    (void)env;
    (void)self;
    return (jint)rn_ffi_local_seconds(unix_s, utc_offset_s);
}
