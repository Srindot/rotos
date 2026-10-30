/*
 * rn_protocol.c - framing, payload codecs and the resyncing stream parser.
 */
#include "rn/rn_protocol.h"

#include "rn/rn_maneuver.h"

#include <math.h>
#include <string.h>

#include "rn/rn_crc.h"
#include "rn/rn_text.h"
#include "rn/rn_wire.h"

#define NAV_STATE_LEN     38u
#define PHONE_STATUS_LEN  14u
#define WEATHER_LEN       10u
#define SETTINGS_LEN       4u
#define MAP_BEGIN_LEN     12u
#define MAP_CHUNK_HDR      6u
#define MAP_END_LEN        8u
#define ACK_LEN            8u
#define BUTTON_LEN         2u
#define DEVICE_STATUS_LEN 12u
#define FORECAST_HDR      10u
#define FORECAST_HOUR      5u
#define CALL_STATE_LEN    32u
#define CALL_ACTION_LEN    4u
#define PREP_STATUS_LEN   10u
#define GUIDANCE_LEN      48u
#define NOTIFY_LEN       168u
#define ROUTE_HDR         17u
#define ROUTE_PT          12u

/* ---- helpers ------------------------------------------------------------- */

static rn_status finish(const RnWriter *w, uint16_t *len)
{
    if (!RN_ASSERT(len != NULL) || !w->ok) {
        return RN_ERR_FULL;
    }
    *len = (uint16_t)w->pos;
    return RN_OK;
}

static bool payload_ok(const RnPacket *p, uint8_t type, uint16_t min_len)
{
    if (!RN_ASSERT(p != NULL)) {
        return false;
    }
    return p->type == type && p->len >= min_len && p->len <= RN_PKT_MAX_PAYLOAD;
}

static uint16_t clamp_u16(float v)
{
    if (!(v > 0.0f)) {
        return 0u;
    }
    return (v >= 65535.0f) ? 65535u : (uint16_t)v;
}

/* ---- framing --------------------------------------------------------------- */

rn_status rn_frame_encode(uint8_t type, uint16_t seq, const uint8_t *payload,
                          uint16_t len, uint8_t *out, uint32_t cap,
                          uint32_t *out_len)
{
    if (!RN_ASSERT(out != NULL && out_len != NULL) ||
        !RN_ASSERT(payload != NULL || len == 0u) ||
        !RN_ASSERT(len <= RN_PKT_MAX_PAYLOAD)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u8(&w, RN_SYNC0);
    rn_wr_u8(&w, RN_SYNC1);
    rn_wr_u8(&w, RN_PROTO_VERSION);
    rn_wr_u8(&w, type);
    rn_wr_u16(&w, seq);
    rn_wr_u16(&w, len);
    rn_wr_bytes(&w, payload, len);
    if (!w.ok) {
        return RN_ERR_FULL;
    }
    uint16_t crc = rn_crc16(&out[2], 6u + (uint32_t)len, RN_CRC16_INIT);
    rn_wr_u16(&w, crc);
    if (!w.ok) {
        return RN_ERR_FULL;
    }
    *out_len = w.pos;
    return RN_OK;
}

/* ---- NAV_STATE ------------------------------------------------------------- */

rn_status rn_nav_state_encode(const RnNavState *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    float hdg = fmodf(s->heading_deg, 360.0f);
    if (hdg < 0.0f) {
        hdg += 360.0f;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->t_ms);
    rn_wr_u32(&w, s->route_id);
    rn_wr_i32(&w, (int32_t)lround(s->lat * 1e7));
    rn_wr_i32(&w, (int32_t)lround(s->lon * 1e7));
    rn_wr_u16(&w, clamp_u16(s->speed_mps * 100.0f));
    rn_wr_u16(&w, (uint16_t)(clamp_u16(hdg * 100.0f) % 36000u));
    rn_wr_f32(&w, s->along_m);
    rn_wr_f32(&w, s->remaining_m);
    rn_wr_u32(&w, s->remaining_s);
    rn_wr_u32(&w, s->eta_s % 86400u);
    rn_wr_u8(&w, s->flags);
    rn_wr_u8(&w, s->accuracy_m);
    return finish(&w, len);
}

rn_status rn_nav_state_decode(const RnPacket *p, RnNavState *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_NAV_STATE, NAV_STATE_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->t_ms = rn_rd_u32(&r);
    out->route_id = rn_rd_u32(&r);
    out->lat = (double)rn_rd_i32(&r) * 1e-7;
    out->lon = (double)rn_rd_i32(&r) * 1e-7;
    out->speed_mps = (float)rn_rd_u16(&r) * 0.01f;
    out->heading_deg = (float)rn_rd_u16(&r) * 0.01f;
    out->along_m = rn_rd_f32(&r);
    out->remaining_m = rn_rd_f32(&r);
    out->remaining_s = rn_rd_u32(&r);
    out->eta_s = rn_rd_u32(&r);
    out->flags = rn_rd_u8(&r);
    out->accuracy_m = rn_rd_u8(&r);
    bool finite = isfinite(out->along_m) && isfinite(out->remaining_m);
    return (r.ok && finite) ? RN_OK : RN_ERR_FORMAT;
}

/* ---- PHONE_STATUS ------------------------------------------------------------ */

rn_status rn_phone_status_encode(const RnPhoneStatus *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->t_ms);
    rn_wr_u32(&w, s->time_s % 86400u);
    rn_wr_u8(&w, s->weekday);
    rn_wr_u8(&w, s->day);
    rn_wr_u8(&w, s->month);
    rn_wr_u8(&w, s->battery_pct);
    rn_wr_u8(&w, s->flags);
    rn_wr_u8(&w, s->signal_bars);
    return finish(&w, len);
}

rn_status rn_phone_status_decode(const RnPacket *p, RnPhoneStatus *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_PHONE_STATUS, PHONE_STATUS_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->t_ms = rn_rd_u32(&r);
    out->time_s = rn_rd_u32(&r) % 86400u;
    out->weekday = (uint8_t)(rn_rd_u8(&r) % 7u);
    out->day = rn_rd_u8(&r);
    out->month = rn_rd_u8(&r);
    out->battery_pct = rn_rd_u8(&r);
    out->flags = rn_rd_u8(&r);
    out->signal_bars = rn_rd_u8(&r);
    if (out->battery_pct > 100u) {
        out->battery_pct = 100u;
    }
    if (out->signal_bars > 4u) {
        out->signal_bars = 4u;
    }
    return r.ok ? RN_OK : RN_ERR_FORMAT;
}

/* ---- WEATHER ------------------------------------------------------------------ */

rn_status rn_weather_encode(const RnWeather *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    float t10 = s->temp_c * 10.0f;
    t10 = (t10 > 32767.0f) ? 32767.0f : ((t10 < -32768.0f) ? -32768.0f : t10);
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->t_ms);
    rn_wr_i16(&w, (int16_t)lroundf(t10));
    rn_wr_u8(&w, s->code);
    rn_wr_u8(&w, s->rain_prob);
    rn_wr_u8(&w, s->wind_kmh);
    rn_wr_u8(&w, s->is_day ? 1u : 0u);
    return finish(&w, len);
}

rn_status rn_weather_decode(const RnPacket *p, RnWeather *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_WEATHER, WEATHER_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->t_ms = rn_rd_u32(&r);
    out->temp_c = (float)rn_rd_i16(&r) * 0.1f;
    out->code = rn_rd_u8(&r);
    out->rain_prob = rn_rd_u8(&r);
    out->wind_kmh = rn_rd_u8(&r);
    out->is_day = rn_rd_u8(&r) != 0u;
    return r.ok ? RN_OK : RN_ERR_FORMAT;
}

/* ---- SETTINGS ----------------------------------------------------------------- */

rn_status rn_settings_encode(const RnSettings *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);                 /* clamped: the decoder drops out-of-range packets */
    rn_wr_u8(&w, (s->theme_mode <= RN_THEME_NIGHT) ? s->theme_mode : (uint8_t)RN_THEME_AUTO);
    rn_wr_u8(&w, s->north_up);
    rn_wr_u8(&w, (s->glass_pct <= 100u) ? s->glass_pct : 100u);
    rn_wr_u8(&w, (s->brightness_pct <= 100u) ? s->brightness_pct : 100u);
    return finish(&w, len);
}

rn_status rn_settings_decode(const RnPacket *p, RnSettings *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_SETTINGS, SETTINGS_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->theme_mode = rn_rd_u8(&r);
    out->north_up = rn_rd_u8(&r);
    out->glass_pct = rn_rd_u8(&r);
    out->brightness_pct = rn_rd_u8(&r);
    bool valid = out->theme_mode <= RN_THEME_NIGHT && out->glass_pct <= 100u &&
                 out->brightness_pct <= 100u;
    return (r.ok && valid) ? RN_OK : RN_ERR_FORMAT;
}

/* ---- FORECAST ----------------------------------------------------------------- */

static int16_t temp10(float t)
{
    float v = t * 10.0f;
    v = (v > 32767.0f) ? 32767.0f : ((v < -32768.0f) ? -32768.0f : v);
    return (int16_t)lroundf(v);
}

rn_status rn_forecast_encode(const RnForecast *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL) || !RN_ASSERT(s->n <= RN_FORECAST_HOURS)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->t_ms);
    rn_wr_u8(&w, (uint8_t)(s->start_hour % 24u));
    rn_wr_u8(&w, s->n);
    rn_wr_u16(&w, (uint16_t)((s->sunrise_min < 1440u) ? s->sunrise_min : 1439u));   /* decoder needs < 1440 */
    rn_wr_u16(&w, (uint16_t)((s->sunset_min < 1440u) ? s->sunset_min : 1439u));
    for (uint32_t i = 0u; i < s->n; i++) {
        rn_wr_i16(&w, temp10(s->hours[i].temp_c));
        rn_wr_u8(&w, s->hours[i].code);
        rn_wr_u8(&w, s->hours[i].rain_prob);
        rn_wr_u8(&w, s->hours[i].wind_kmh);
    }
    return finish(&w, len);
}

rn_status rn_forecast_decode(const RnPacket *p, RnForecast *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_FORECAST, FORECAST_HDR)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    (void)memset(out, 0, sizeof *out);
    out->t_ms = rn_rd_u32(&r);
    out->start_hour = (uint8_t)(rn_rd_u8(&r) % 24u);
    out->n = rn_rd_u8(&r);
    out->sunrise_min = rn_rd_u16(&r);
    out->sunset_min = rn_rd_u16(&r);
    if (out->n > RN_FORECAST_HOURS || (uint32_t)p->len < FORECAST_HDR + FORECAST_HOUR * (uint32_t)out->n) {
        return RN_ERR_FORMAT;
    }
    for (uint32_t i = 0u; i < out->n; i++) {
        out->hours[i].temp_c = (float)rn_rd_i16(&r) * 0.1f;
        out->hours[i].code = rn_rd_u8(&r);
        out->hours[i].rain_prob = rn_rd_u8(&r);
        out->hours[i].wind_kmh = rn_rd_u8(&r);
    }
    bool valid = out->sunrise_min < 1440u && out->sunset_min < 1440u;
    return (r.ok && valid) ? RN_OK : RN_ERR_FORMAT;
}

/* ---- CALL_STATE / CALL_ACTION ------------------------------------------------------ */

rn_status rn_call_state_encode(const RnCallState *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    uint8_t name[RN_CALL_NAME_MAX];
    char clean[RN_CALL_NAME_MAX + 1u];
    (void)rn_text_ascii_upper(clean, (uint32_t)sizeof clean, s->name);
    (void)memset(name, 0, sizeof name);
    (void)memcpy(name, clean, strlen(clean));
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->t_ms);
    rn_wr_u16(&w, s->call_id);
    rn_wr_u8(&w, s->state);
    rn_wr_u8(&w, s->flags);
    rn_wr_bytes(&w, name, RN_CALL_NAME_MAX);
    return finish(&w, len);
}

rn_status rn_call_state_decode(const RnPacket *p, RnCallState *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_CALL_STATE, CALL_STATE_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->t_ms = rn_rd_u32(&r);
    out->call_id = rn_rd_u16(&r);
    out->state = rn_rd_u8(&r);
    out->flags = rn_rd_u8(&r);
    rn_rd_bytes(&r, (uint8_t *)out->name, RN_CALL_NAME_MAX);
    out->name[RN_CALL_NAME_MAX] = '\0';
    return (r.ok && out->state <= RN_CALL_ACTIVE) ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_guidance_encode(const RnGuidance *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    uint8_t road[RN_GUIDE_ROAD_MAX];
    char clean[RN_GUIDE_ROAD_MAX + 1u];
    (void)rn_text_ascii_upper(clean, (uint32_t)sizeof clean, s->road);
    (void)memset(road, 0, sizeof road);
    (void)memcpy(road, clean, strlen(clean));
    uint8_t man = (s->maneuver < (uint8_t)RN_M_COUNT) ? s->maneuver : (uint8_t)RN_M_STRAIGHT;
    uint32_t eta = (s->eta_s < 86400u) ? s->eta_s : RN_GUIDE_UNKNOWN32;   /* decoder rules */
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->t_ms);
    rn_wr_u8(&w, man);
    rn_wr_u8(&w, s->flags);
    rn_wr_u16(&w, s->to_turn_m);
    rn_wr_u32(&w, s->remaining_m);
    rn_wr_u32(&w, eta);
    rn_wr_bytes(&w, road, RN_GUIDE_ROAD_MAX);
    return finish(&w, len);
}

rn_status rn_guidance_decode(const RnPacket *p, RnGuidance *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_GUIDANCE, GUIDANCE_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->t_ms = rn_rd_u32(&r);
    out->maneuver = rn_rd_u8(&r);
    out->flags = rn_rd_u8(&r);
    out->to_turn_m = rn_rd_u16(&r);
    out->remaining_m = rn_rd_u32(&r);
    out->eta_s = rn_rd_u32(&r);
    rn_rd_bytes(&r, (uint8_t *)out->road, RN_GUIDE_ROAD_MAX);
    out->road[RN_GUIDE_ROAD_MAX] = '\0';
    bool eta_ok = out->eta_s == RN_GUIDE_UNKNOWN32 || out->eta_s < 86400u;
    return (r.ok && out->maneuver < (uint8_t)RN_M_COUNT && eta_ok) ? RN_OK : RN_ERR_FORMAT;
}

/* s as upper-case ASCII in exactly n bytes, NUL-padded (n <= RN_NOTIFY_TEXT_MAX). */
static void wr_text(RnWriter *w, const char *s, uint32_t n)
{
    char clean[RN_NOTIFY_TEXT_MAX + 1u];
    uint8_t field[RN_NOTIFY_TEXT_MAX];
    uint32_t k = (n < RN_NOTIFY_TEXT_MAX) ? n : RN_NOTIFY_TEXT_MAX;
    (void)rn_text_ascii_upper(clean, k + 1u, (s != NULL) ? s : "");
    (void)memset(field, 0, sizeof field);
    (void)memcpy(field, clean, strlen(clean));
    rn_wr_bytes(w, field, k);
}

static void rd_text(RnReader *r, char *out, uint32_t n)
{
    rn_rd_bytes(r, (uint8_t *)out, n);
    out[n] = '\0';
}

rn_status rn_route_chunk_encode(const RnRouteChunk *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL) || !RN_ASSERT(s->n >= 1u && s->n <= RN_ROUTE_CHUNK_PTS) ||
        !RN_ASSERT((uint32_t)s->first + s->n <= s->total && s->total <= RN_ROUTE_MAX_PTS)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->route_id);
    rn_wr_u32(&w, s->pack_route_id);
    rn_wr_u16(&w, s->total);
    rn_wr_u16(&w, s->first);
    rn_wr_u8(&w, s->n);
    rn_wr_f32(&w, s->total_m);
    for (uint32_t i = 0u; i < s->n; i++) {
        rn_wr_f32(&w, s->x[i]);
        rn_wr_f32(&w, s->y[i]);
        rn_wr_f32(&w, s->along[i]);
    }
    return finish(&w, len);
}

static bool route_values_ok(const RnRouteChunk *c)
{
    bool ok = isfinite(c->total_m);
    for (uint32_t i = 0u; i < c->n; i++) {
        ok = ok && isfinite(c->x[i]) && isfinite(c->y[i]) && isfinite(c->along[i]);
    }
    return ok;
}

rn_status rn_route_chunk_decode(const RnPacket *p, RnRouteChunk *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_ROUTE, ROUTE_HDR + ROUTE_PT)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->route_id = rn_rd_u32(&r);
    out->pack_route_id = rn_rd_u32(&r);
    out->total = rn_rd_u16(&r);
    out->first = rn_rd_u16(&r);
    out->n = rn_rd_u8(&r);
    out->total_m = rn_rd_f32(&r);
    if (!r.ok || out->n < 1u || out->n > RN_ROUTE_CHUNK_PTS || out->total > RN_ROUTE_MAX_PTS ||
        (uint32_t)out->first + out->n > out->total || p->len != ROUTE_HDR + ROUTE_PT * (uint32_t)out->n) {
        return RN_ERR_FORMAT;
    }
    for (uint32_t i = 0u; i < out->n; i++) {
        out->x[i] = rn_rd_f32(&r);
        out->y[i] = rn_rd_f32(&r);
        out->along[i] = rn_rd_f32(&r);
    }
    return (r.ok && route_values_ok(out)) ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_notify_encode(const RnNotify *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->t_ms);
    rn_wr_u16(&w, s->id);
    rn_wr_u8(&w, (s->kind <= RN_NOTIFY_OTHER) ? s->kind : (uint8_t)RN_NOTIFY_OTHER);
    rn_wr_u8(&w, 0u);
    wr_text(&w, s->app, RN_NOTIFY_APP_MAX);
    wr_text(&w, s->title, RN_NOTIFY_TITLE_MAX);
    wr_text(&w, s->text, RN_NOTIFY_TEXT_MAX);
    return finish(&w, len);
}

rn_status rn_notify_decode(const RnPacket *p, RnNotify *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_NOTIFY, NOTIFY_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->t_ms = rn_rd_u32(&r);
    out->id = rn_rd_u16(&r);
    out->kind = rn_rd_u8(&r);
    rn_rd_skip(&r, 1u);
    rd_text(&r, out->app, RN_NOTIFY_APP_MAX);
    rd_text(&r, out->title, RN_NOTIFY_TITLE_MAX);
    rd_text(&r, out->text, RN_NOTIFY_TEXT_MAX);
    return (r.ok && out->kind <= RN_NOTIFY_OTHER) ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_prep_status_encode(const RnPrepStatus *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);                 /* clamped to what the decoder accepts */
    rn_wr_u32(&w, s->t_ms);
    rn_wr_u8(&w, (s->stage <= RN_PREP_STAGE_MAX) ? s->stage : (uint8_t)RN_PREP_FAILED);
    rn_wr_u8(&w, (s->pct > 100u) ? 100u : s->pct);
    rn_wr_u16(&w, (s->done <= s->total) ? s->done : s->total);
    rn_wr_u16(&w, s->total);
    return finish(&w, len);
}

rn_status rn_prep_status_decode(const RnPacket *p, RnPrepStatus *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_PREP_STATUS, PREP_STATUS_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->t_ms = rn_rd_u32(&r);
    out->stage = rn_rd_u8(&r);
    out->pct = rn_rd_u8(&r);
    out->done = rn_rd_u16(&r);
    out->total = rn_rd_u16(&r);
    bool valid = out->stage <= RN_PREP_STAGE_MAX && out->pct <= 100u && out->done <= out->total;
    return (r.ok && valid) ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_call_action_encode(const RnCallAction *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u16(&w, s->call_id);
    rn_wr_u8(&w, s->action);
    rn_wr_u8(&w, 0u);
    return finish(&w, len);
}

rn_status rn_call_action_decode(const RnPacket *p, RnCallAction *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_CALL_ACTION, CALL_ACTION_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->call_id = rn_rd_u16(&r);
    out->action = rn_rd_u8(&r);
    bool valid = out->action == RN_ACTION_REJECT_REPLY || out->action == RN_ACTION_DISMISS;
    return (r.ok && valid) ? RN_OK : RN_ERR_FORMAT;
}

/* ---- map transfer ---------------------------------------------------------------- */

rn_status rn_map_begin_encode(const RnMapBegin *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->total_len);
    rn_wr_u32(&w, s->crc32);
    rn_wr_u32(&w, s->route_id);
    rn_wr_u32(&w, s->wire_len);
    rn_wr_u32(&w, s->base_crc);
    return finish(&w, len);
}

rn_status rn_map_begin_decode(const RnPacket *p, RnMapBegin *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_MAP_BEGIN, MAP_BEGIN_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->total_len = rn_rd_u32(&r);
    out->crc32 = rn_rd_u32(&r);
    out->route_id = rn_rd_u32(&r);
    out->wire_len = (p->len >= MAP_BEGIN_LEN + 4u) ? rn_rd_u32(&r) : 0u;
    out->base_crc = (p->len >= MAP_BEGIN_LEN + 8u) ? rn_rd_u32(&r) : 0u;
    return r.ok ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_map_chunk_encode(const RnMapChunk *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL) || !RN_ASSERT(s->n <= RN_MAP_CHUNK_DATA)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->offset);
    rn_wr_u16(&w, s->n);
    rn_wr_bytes(&w, s->data, s->n);
    return finish(&w, len);
}

rn_status rn_map_chunk_decode(const RnPacket *p, RnMapChunk *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_MAP_CHUNK, MAP_CHUNK_HDR)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->offset = rn_rd_u32(&r);
    out->n = rn_rd_u16(&r);
    if (out->n > RN_MAP_CHUNK_DATA || (uint32_t)p->len != MAP_CHUNK_HDR + (uint32_t)out->n) {
        return RN_ERR_FORMAT;
    }
    rn_rd_bytes(&r, out->data, out->n);
    return r.ok ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_map_end_encode(const RnMapEnd *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->total_len);
    rn_wr_u32(&w, s->crc32);
    return finish(&w, len);
}

rn_status rn_map_end_decode(const RnPacket *p, RnMapEnd *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_MAP_END, MAP_END_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->total_len = rn_rd_u32(&r);
    out->crc32 = rn_rd_u32(&r);
    return r.ok ? RN_OK : RN_ERR_FORMAT;
}

/* ---- ACK / BUTTON ------------------------------------------------------------------ */

rn_status rn_ack_encode(const RnAck *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u8(&w, s->acked_type);
    rn_wr_u8(&w, s->status);
    rn_wr_u16(&w, s->acked_seq);
    rn_wr_u32(&w, s->value);
    return finish(&w, len);
}

rn_status rn_ack_decode(const RnPacket *p, RnAck *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_ACK, ACK_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->acked_type = rn_rd_u8(&r);
    out->status = rn_rd_u8(&r);
    out->acked_seq = rn_rd_u16(&r);
    out->value = rn_rd_u32(&r);
    return r.ok ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_button_encode(const RnButton *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u8(&w, s->button);
    rn_wr_u8(&w, s->event);
    return finish(&w, len);
}

rn_status rn_button_decode(const RnPacket *p, RnButton *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_BUTTON, BUTTON_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->button = rn_rd_u8(&r);
    out->event = rn_rd_u8(&r);
    return r.ok ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_device_status_encode(const RnDeviceStatus *s, uint8_t *out, uint32_t cap, uint16_t *len)
{
    if (!RN_ASSERT(s != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    RnWriter w = rn_writer(out, cap);
    rn_wr_u32(&w, s->uptime_ms);
    rn_wr_u8(&w, s->reset_reason);
    rn_wr_u8(&w, s->flags);
    rn_wr_u16(&w, 0u);
    rn_wr_u32(&w, s->off_s);
    return finish(&w, len);
}

rn_status rn_device_status_decode(const RnPacket *p, RnDeviceStatus *out)
{
    if (!RN_ASSERT(out != NULL) || !payload_ok(p, RN_PKT_DEVICE_STATUS, DEVICE_STATUS_LEN)) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(p->payload, p->len);
    out->uptime_ms = rn_rd_u32(&r);
    out->reset_reason = rn_rd_u8(&r);
    out->flags = rn_rd_u8(&r);
    (void)rn_rd_u16(&r);
    out->off_s = rn_rd_u32(&r);
    return r.ok ? RN_OK : RN_ERR_FORMAT;
}

/* ---- stream parser ----------------------------------------------------------------
 *
 * buf[0..have) holds the bytes of the frame candidate being assembled,
 * starting with the sync pair. When a candidate turns out to be garbage
 * (bad header or CRC) the parser does not throw the bytes away: it slides to
 * the next A5 5A inside them and re-examines from there, so a fake sync in
 * line noise can never swallow the real frame that follows it.
 */

void rn_parser_init(RnParser *p)
{
    if (!RN_ASSERT(p != NULL)) {
        return;
    }
    (void)memset(p, 0, sizeof *p);
    p->last_seq = -1;
}

/* Sequence bookkeeping: forward gaps are losses; a late (reordered) packet
 * gives back one loss it had been counted as. */
static void count_seq(RnParser *p, uint16_t seq)
{
    if (p->last_seq < 0) {
        p->last_seq = (int32_t)seq;
        return;
    }
    uint16_t gap = (uint16_t)(seq - (uint16_t)p->last_seq);
    if (gap > 0u && gap < 0x8000u) {
        p->dropped += (uint32_t)gap - 1u;
        p->last_seq = (int32_t)seq;
    } else if (gap >= 0x8000u && p->dropped > 0u) {
        p->dropped--;
    } else {
        /* duplicate: nothing to count */
    }
}

/* Drop the first `k` buffered bytes. */
static void shift(RnParser *p, uint32_t k)
{
    uint32_t n = (k < p->have) ? p->have - k : 0u;
    if (n > 0u) {
        (void)memmove(p->buf, &p->buf[k], n);
    }
    p->have = n;
}

/* Discard the current candidate: keep from the next possible sync. */
static void resync(RnParser *p)
{
    uint32_t k = 1u;
    while (k < p->have) {
        bool sync = p->buf[k] == RN_SYNC0 && (k + 1u == p->have || p->buf[k + 1u] == RN_SYNC1);
        if (sync) {
            break;
        }
        k++;
    }
    shift(p, k);
}

static void emit(RnParser *p, uint32_t len, RnPacket *out)
{
    out->version = p->buf[2];
    out->type = p->buf[3];
    out->seq = rn_get_u16(p->buf, 4u);
    out->len = (uint16_t)len;
    (void)memcpy(out->payload, &p->buf[RN_PKT_HEADER], len);
    count_seq(p, out->seq);
    p->ok++;
}

typedef enum { STEP_NEED_MORE, STEP_REJECT, STEP_PACKET } Step;

/* Examine the candidate at the front of the buffer. */
static Step examine(RnParser *p, RnPacket *out)
{
    if (p->have >= 1u && p->buf[0] != RN_SYNC0) {
        return STEP_REJECT;
    }
    if (p->have >= 2u && p->buf[1] != RN_SYNC1) {
        return STEP_REJECT;
    }
    if (p->have < RN_PKT_HEADER) {
        return STEP_NEED_MORE;
    }
    uint32_t len = rn_get_u16(p->buf, 6u);
    if (p->buf[2] != RN_PROTO_VERSION || len > RN_PKT_MAX_PAYLOAD) {
        p->crc_errors++;
        return STEP_REJECT;
    }
    uint32_t need = RN_PKT_HEADER + len + RN_PKT_TRAILER;
    if (p->have < need) {
        return STEP_NEED_MORE;
    }
    uint16_t crc = rn_get_u16(p->buf, need - 2u);
    if (rn_crc16(&p->buf[2], 6u + len, RN_CRC16_INIT) != crc) {
        p->crc_errors++;
        return STEP_REJECT;
    }
    emit(p, len, out);
    shift(p, need);
    return STEP_PACKET;
}

/* Run the state machine over the buffer; at most one packet per call. */
static RnParseResult advance(RnParser *p, RnPacket *out)
{
    bool rejected = false;
    for (uint32_t guard = 0u; guard <= RN_PKT_MAX_FRAME; guard++) {
        Step st = examine(p, out);
        if (st == STEP_PACKET) {
            return RN_PARSE_PACKET;
        }
        if (st == STEP_NEED_MORE) {
            break;
        }
        rejected = rejected || p->have >= 2u;
        resync(p);
    }
    return rejected ? RN_PARSE_ERROR : RN_PARSE_MORE;
}

RnParseResult rn_parser_push(RnParser *p, uint8_t byte, RnPacket *out)
{
    if (!RN_ASSERT(p != NULL && out != NULL) || !RN_ASSERT(p->have <= RN_PKT_MAX_FRAME)) {
        return RN_PARSE_ERROR;
    }
    if (p->have == RN_PKT_MAX_FRAME) {
        resync(p);                               /* cannot happen; stay safe */
    }
    p->buf[p->have] = byte;
    p->have++;
    return advance(p, out);
}

RnParseResult rn_parser_drain(RnParser *p, RnPacket *out)
{
    if (!RN_ASSERT(p != NULL && out != NULL)) {
        return RN_PARSE_ERROR;
    }
    return advance(p, out);
}
