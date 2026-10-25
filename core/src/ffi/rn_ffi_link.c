/*
 * rn_ffi_link.c - FFI: navigation engine, outbound frames, inbound parser.
 */
#include <math.h>
#include <string.h>

#include "rn/rn_text.h"
#include "rn_ffi_ctx.h"

/* Encode payload + frame into out; returns frame length or an error. */
static int32_t frame(uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t len,
                     uint8_t *out, int32_t cap)
{
    if (!RN_ASSERT(out != NULL) || cap <= 0) {
        return RN_ERR_PARAM;
    }
    uint32_t n = 0u;
    rn_status st = rn_frame_encode(type, seq, payload, len, out, (uint32_t)cap, &n);
    return (st == RN_OK) ? (int32_t)n : st;
}

void rn_ffi_engine_reset(void)
{
    rn_engine_init(&rn_ffi_ctx()->engine);
    rn_ffi_ctx()->have_nav = false;
}

int32_t rn_ffi_engine_fix(uint16_t seq, uint32_t t_ms, double unix_s, int32_t utc_offset_s,
                          double lat, double lon, float speed_mps, float heading_deg,
                          float accuracy_m, uint8_t *out, int32_t cap)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(out != NULL) || c->route.n_pts < 2u) {
        return RN_ERR_STATE;
    }
    RnFix fix = {t_ms, unix_s, utc_offset_s, lat, lon, speed_mps, heading_deg, accuracy_m};
    rn_status st = rn_engine_on_fix(&c->engine, &c->route, &fix, &c->nav);
    if (st != RN_OK) {
        return st;
    }
    c->have_nav = true;
    uint8_t payload[RN_PKT_MAX_PAYLOAD];
    uint16_t len = 0u;
    st = rn_nav_state_encode(&c->nav, payload, (uint32_t)sizeof payload, &len);
    if (st != RN_OK) {
        return st;
    }
    return frame(RN_PKT_NAV_STATE, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_position(uint16_t seq, uint32_t t_ms, uint32_t route_id, double lat, double lon,
                              float speed_mps, float heading_deg, float accuracy_m, float along_m,
                              float remaining_m, uint32_t eta_s, uint8_t flags, uint8_t *out, int32_t cap)
{
    if (!RN_ASSERT(out != NULL) || !RN_ASSERT(lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0)) {
        return RN_ERR_PARAM;
    }
    RnNavState s;
    (void)memset(&s, 0, sizeof s);
    s.t_ms = t_ms;
    s.route_id = route_id;
    s.lat = lat;
    s.lon = lon;
    s.speed_mps = (speed_mps > 0.0f) ? speed_mps : 0.0f;
    s.heading_deg = heading_deg;
    s.along_m = (isfinite(along_m) && along_m > 0.0f) ? along_m : 0.0f;
    s.remaining_m = (isfinite(remaining_m) && remaining_m > 0.0f) ? remaining_m : 0.0f;
    s.speed_mps = isfinite(speed_mps) ? s.speed_mps : 0.0f;
    s.heading_deg = isfinite(heading_deg) ? heading_deg : 0.0f;
    s.eta_s = eta_s;
    s.flags = (uint8_t)(flags | RN_NAV_FREE);
    s.accuracy_m = (accuracy_m < 255.0f && accuracy_m >= 0.0f) ? (uint8_t)accuracy_m : 255u;
    uint8_t payload[RN_PKT_MAX_PAYLOAD];
    uint16_t len = 0u;
    rn_status st = rn_nav_state_encode(&s, payload, (uint32_t)sizeof payload, &len);
    if (st != RN_OK) {
        return st;
    }
    return frame(RN_PKT_NAV_STATE, seq, payload, len, out, cap);
}

int32_t rn_ffi_engine_state(float *along_m, float *remaining_m, uint32_t *remaining_s,
                            uint8_t *flags, int32_t *wants_reroute)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(along_m != NULL && remaining_m != NULL && remaining_s != NULL) ||
        !RN_ASSERT(flags != NULL && wants_reroute != NULL)) {
        return RN_ERR_PARAM;
    }
    if (!c->have_nav) {
        return RN_ERR_STATE;
    }
    *along_m = c->nav.along_m;
    *remaining_m = c->nav.remaining_m;
    *remaining_s = c->nav.remaining_s;
    *flags = c->nav.flags;
    *wants_reroute = rn_engine_wants_reroute(&c->engine, c->nav.t_ms) ? 1 : 0;
    return RN_OK;
}

int32_t rn_ffi_frame_phone_status(uint16_t seq, uint32_t t_ms, uint32_t time_s, uint8_t weekday,
                                  uint8_t day, uint8_t month, uint8_t battery_pct, uint8_t flags,
                                  uint8_t signal_bars, uint8_t *out, int32_t cap)
{
    RnPhoneStatus s = {t_ms, time_s, weekday, day, month, battery_pct, flags, signal_bars};
    uint8_t payload[32];
    uint16_t len = 0u;
    if (rn_phone_status_encode(&s, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_PHONE_STATUS, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_prep_status(uint16_t seq, uint32_t t_ms, uint8_t stage, uint8_t pct,
                                 uint16_t done, uint16_t total, uint8_t *out, int32_t cap)
{
    RnPrepStatus s = {t_ms, stage, pct, done, total};
    uint8_t payload[16];
    uint16_t len = 0u;
    if (rn_prep_status_encode(&s, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_PREP_STATUS, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_guidance(uint16_t seq, uint32_t t_ms, uint8_t maneuver, uint8_t flags,
                              uint16_t to_turn_m, uint32_t remaining_m, uint32_t eta_s, const char *road,
                              uint8_t *out, int32_t cap)
{
    if (!RN_ASSERT(road != NULL)) {
        return RN_ERR_PARAM;
    }
    RnGuidance g;
    (void)memset(&g, 0, sizeof g);
    g.t_ms = t_ms;
    g.maneuver = maneuver;
    g.flags = flags;
    g.to_turn_m = to_turn_m;
    g.remaining_m = remaining_m;
    g.eta_s = eta_s;
    (void)rn_text_copy(g.road, (uint32_t)sizeof g.road, road);
    uint8_t payload[64];
    uint16_t len = 0u;
    if (rn_guidance_encode(&g, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_GUIDANCE, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_weather(uint16_t seq, uint32_t t_ms, float temp_c, uint8_t code,
                             uint8_t rain_prob, uint8_t wind_kmh, uint8_t is_day,
                             uint8_t *out, int32_t cap)
{
    RnWeather w = {t_ms, temp_c, code, rain_prob, wind_kmh, is_day != 0u};
    uint8_t payload[32];
    uint16_t len = 0u;
    if (rn_weather_encode(&w, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_WEATHER, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_settings(uint16_t seq, uint8_t theme_mode, uint8_t north_up,
                              uint8_t glass_pct, uint8_t brightness_pct, uint8_t *out, int32_t cap)
{
    RnSettings s = {theme_mode, north_up, glass_pct, brightness_pct};
    uint8_t payload[8];
    uint16_t len = 0u;
    if (rn_settings_encode(&s, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_SETTINGS, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_ping(uint16_t seq, uint8_t *out, int32_t cap)
{
    return frame(RN_PKT_PING, seq, NULL, 0u, out, cap);
}

int32_t rn_ffi_frame_forecast(uint16_t seq, uint32_t t_ms, uint8_t start_hour, uint8_t n,
                              uint16_t sunrise_min, uint16_t sunset_min, const float *temp_c,
                              const uint8_t *code, const uint8_t *rain_prob, const uint8_t *wind_kmh,
                              uint8_t *out, int32_t cap)
{
    if (!RN_ASSERT(temp_c != NULL && code != NULL && rain_prob != NULL && wind_kmh != NULL) ||
        !RN_ASSERT(n <= RN_FORECAST_HOURS)) {
        return RN_ERR_PARAM;
    }
    RnForecast f;
    (void)memset(&f, 0, sizeof f);
    f.t_ms = t_ms;
    f.start_hour = start_hour;
    f.n = n;
    f.sunrise_min = sunrise_min;
    f.sunset_min = sunset_min;
    for (uint32_t i = 0u; i < n; i++) {
        f.hours[i].temp_c = temp_c[i];
        f.hours[i].code = code[i];
        f.hours[i].rain_prob = rain_prob[i];
        f.hours[i].wind_kmh = wind_kmh[i];
    }
    uint8_t payload[RN_PKT_MAX_PAYLOAD];
    uint16_t len = 0u;
    if (rn_forecast_encode(&f, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_FORECAST, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_call_state(uint16_t seq, uint32_t t_ms, uint16_t call_id, uint8_t state,
                                uint8_t flags, const char *name, uint8_t *out, int32_t cap)
{
    if (!RN_ASSERT(state <= RN_CALL_ACTIVE)) {
        return RN_ERR_PARAM;
    }
    RnCallState c;
    (void)memset(&c, 0, sizeof c);
    c.t_ms = t_ms;
    c.call_id = call_id;
    c.state = state;
    c.flags = flags;
    (void)rn_text_ascii_upper(c.name, (uint32_t)sizeof c.name, (name != NULL) ? name : "");
    uint8_t payload[64];
    uint16_t len = 0u;
    if (rn_call_state_encode(&c, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_CALL_STATE, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_notify(uint16_t seq, uint32_t t_ms, uint16_t id, uint8_t kind, const char *app,
                            const char *title, const char *text, uint8_t *out, int32_t cap)
{
    static RnNotify n;                            /* 170 B: keep it off the stack */
    n.t_ms = t_ms;
    n.id = id;
    n.kind = kind;
    (void)rn_text_ascii_upper(n.app, (uint32_t)sizeof n.app, (app != NULL) ? app : "");
    (void)rn_text_ascii_upper(n.title, (uint32_t)sizeof n.title, (title != NULL) ? title : "");
    (void)rn_text_ascii_upper(n.text, (uint32_t)sizeof n.text, (text != NULL) ? text : "");
    uint8_t payload[RN_PKT_MAX_PAYLOAD];
    uint16_t len = 0u;
    if (rn_notify_encode(&n, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_NOTIFY, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_map_begin(uint16_t seq, uint8_t *out, int32_t cap)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!c->pack_ok) {
        return RN_ERR_STATE;
    }
    RnMapBegin b = {c->pack_len, c->pack_crc, c->pack_view.h.route_id, c->wire_len, c->wire_base_crc};
    uint8_t payload[24];
    uint16_t len = 0u;
    if (rn_map_begin_encode(&b, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_MAP_BEGIN, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_map_chunk(uint16_t seq, uint32_t offset, uint8_t *out, int32_t cap)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!c->pack_ok) {
        return RN_ERR_STATE;
    }
    const uint8_t *src = (c->wire_len > 0u) ? c->wire : c->pack;
    uint32_t total = (c->wire_len > 0u) ? c->wire_len : c->pack_len;
    if (offset >= total) {
        return 0;
    }
    static RnMapChunk chunk;
    uint32_t n = total - offset;
    chunk.offset = offset;
    chunk.n = (uint16_t)((n > RN_MAP_CHUNK_DATA) ? RN_MAP_CHUNK_DATA : n);
    (void)memcpy(chunk.data, &src[offset], chunk.n);
    uint8_t payload[RN_PKT_MAX_PAYLOAD];
    uint16_t len = 0u;
    if (rn_map_chunk_encode(&chunk, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_MAP_CHUNK, seq, payload, len, out, cap);
}

int32_t rn_ffi_frame_map_end(uint16_t seq, uint8_t *out, int32_t cap)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!c->pack_ok) {
        return RN_ERR_STATE;
    }
    RnMapEnd e = {c->pack_len, c->pack_crc};
    uint8_t payload[16];
    uint16_t len = 0u;
    if (rn_map_end_encode(&e, payload, (uint32_t)sizeof payload, &len) != RN_OK) {
        return RN_ERR_FULL;
    }
    return frame(RN_PKT_MAP_END, seq, payload, len, out, cap);
}

int32_t rn_ffi_map_chunk_size(void)
{
    return (int32_t)RN_MAP_CHUNK_DATA;
}

void rn_ffi_rx_reset(void)
{
    RnFfiCtx *c = rn_ffi_ctx();
    rn_parser_init(&c->rx);
    c->rx_head = 0u;
    c->rx_count = 0u;
}

/* Queue slot the parser writes into next (the oldest is dropped when full). */
static RnPacket *rx_slot(RnFfiCtx *c)
{
    return &c->rxq[(c->rx_head + c->rx_count) % RN_FFI_RX_QUEUE];
}

static void rx_commit(RnFfiCtx *c)
{
    if (c->rx_count < RN_FFI_RX_QUEUE) {
        c->rx_count++;
    } else {
        c->rx_head = (c->rx_head + 1u) % RN_FFI_RX_QUEUE;
    }
}

int32_t rn_ffi_rx_feed(const uint8_t *bytes, int32_t n)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(bytes != NULL || n == 0) || n < 0) {
        return RN_ERR_PARAM;
    }
    int32_t queued = 0;
    for (int32_t i = 0; i < n; i++) {
        RnParseResult res = rn_parser_push(&c->rx, bytes[i], rx_slot(c));
        /* the queue drops its oldest entry when full, so commit every packet drained */
        for (uint32_t k = 0u; k < RN_PKT_MAX_FRAME && res == RN_PARSE_PACKET; k++) {
            rx_commit(c);
            queued++;
            res = rn_parser_drain(&c->rx, rx_slot(c));
        }
    }
    return queued;
}

int32_t rn_ffi_rx_next(uint8_t *type, uint16_t *seq, uint8_t *payload, int32_t cap)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(type != NULL && seq != NULL && payload != NULL) || c->rx_count == 0u) {
        return -1;
    }
    const RnPacket *p = &c->rxq[c->rx_head];
    if (cap < 0 || (uint32_t)cap < p->len) {
        return RN_ERR_FULL;
    }
    *type = p->type;
    *seq = p->seq;
    (void)memcpy(payload, p->payload, p->len);
    c->rx_head = (c->rx_head + 1u) % RN_FFI_RX_QUEUE;
    c->rx_count--;
    return (int32_t)p->len;
}

static bool to_packet(RnPacket *p, uint8_t type, const uint8_t *payload, int32_t len)
{
    if (!RN_ASSERT(payload != NULL) || len < 0 || (uint32_t)len > RN_PKT_MAX_PAYLOAD) {
        return false;
    }
    (void)memset(p, 0, sizeof *p);
    p->type = type;
    p->len = (uint16_t)len;
    (void)memcpy(p->payload, payload, (uint32_t)len);
    return true;
}

int32_t rn_ffi_decode_ack(const uint8_t *payload, int32_t len, uint8_t *acked_type,
                          uint8_t *status, uint16_t *acked_seq, uint32_t *value)
{
    static RnPacket p;
    RnAck a;
    if (!RN_ASSERT(acked_type != NULL && status != NULL && acked_seq != NULL && value != NULL) ||
        !to_packet(&p, RN_PKT_ACK, payload, len) || rn_ack_decode(&p, &a) != RN_OK) {
        return RN_ERR_FORMAT;
    }
    *acked_type = a.acked_type;
    *status = a.status;
    *acked_seq = a.acked_seq;
    *value = a.value;
    return RN_OK;
}

int32_t rn_ffi_decode_button(const uint8_t *payload, int32_t len, uint8_t *button, uint8_t *event)
{
    static RnPacket p;
    RnButton b;
    if (!RN_ASSERT(button != NULL && event != NULL) ||
        !to_packet(&p, RN_PKT_BUTTON, payload, len) || rn_button_decode(&p, &b) != RN_OK) {
        return RN_ERR_FORMAT;
    }
    *button = b.button;
    *event = b.event;
    return RN_OK;
}

int32_t rn_ffi_decode_call_action(const uint8_t *payload, int32_t len, uint16_t *call_id, uint8_t *action)
{
    static RnPacket p;
    RnCallAction a;
    if (!RN_ASSERT(call_id != NULL && action != NULL) ||
        !to_packet(&p, RN_PKT_CALL_ACTION, payload, len) || rn_call_action_decode(&p, &a) != RN_OK) {
        return RN_ERR_FORMAT;
    }
    *call_id = a.call_id;
    *action = a.action;
    return RN_OK;
}
