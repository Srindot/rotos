/*
 * rn_ffi.h - flat C API of the navigation library for foreign callers:
 * the Android app (JNI, apk/app/src/main/cpp/rotos_jni.c) and the
 * simulator's virtual phone (Python ctypes, navigation/python/rotos_nav.py).
 *
 * One navigation session per process, held in static memory (JPL rule 3):
 * a route, its flyover data, its corridor, the map pack being built, the
 * navigation engine, an inbound packet parser and a ride simulator.
 *
 * Conventions: only plain C types; int32_t results are >= 0 on success or a
 * negative rn_status; output buffers come with their capacity.
 *
 * Typical phone flow:
 *   rn_ffi_reset()
 *   per Google step: first = rn_ffi_route_add_polyline(step polyline)
 *                    rn_ffi_route_add_maneuver_google(maneuver, first, instruction)
 *   rn_ffi_route_finish(duration_s, route_id)
 *   rn_ffi_bridges_add(...) for each OSM bridge; rn_ffi_flyover_annotate()
 *   rn_ffi_corridor_plan(16, 260)
 *   per block of cells: rn_ffi_mapgen_block(rgb, ...) then rn_ffi_pack_add_tile(...)
 *   rn_ffi_pack_finish(); MAP_BEGIN / MAP_CHUNK... / MAP_END frames
 *   per GPS fix: rn_ffi_engine_fix(...) -> NAV_STATE frame
 */
#ifndef RN_FFI_H
#define RN_FFI_H

#include "rn/rn_types.h"

#define RN_FFI_API_VERSION   2
#define RN_FFI_PACK_CAP      (8u * 1024u * 1024u)
#define RN_FFI_RX_QUEUE      16u

int32_t rn_ffi_api_version(void);
void rn_ffi_reset(void);
uint32_t rn_ffi_assert_count(void);

/* ---- route ---------------------------------------------------------------- */
/* Appends a step's encoded polyline; returns the route index of the step's
 * first point (junction points shared with the previous step are merged). */
int32_t rn_ffi_route_add_polyline(const char *encoded);
int32_t rn_ffi_route_add_point(double lat, double lon);
int32_t rn_ffi_route_point_count(void);
int32_t rn_ffi_route_add_maneuver(int32_t type, int32_t point_idx, const char *road);
/* Google maneuver string + instruction text; the road name is extracted. */
int32_t rn_ffi_route_add_maneuver_google(const char *maneuver, int32_t point_idx,
                                         const char *instruction);
int32_t rn_ffi_route_finish(float duration_s, uint32_t route_id);
float rn_ffi_route_total_m(void);
float rn_ffi_route_duration_s(void);
int32_t rn_ffi_route_maneuver_count(void);
/* road: at least RN_ROAD_NAME_MAX + 1 bytes. */
int32_t rn_ffi_route_maneuver(int32_t i, uint8_t *type, uint8_t *hint, float *along_m, char *road);
int32_t rn_ffi_route_point(int32_t i, double *lat, double *lon, float *along_m);

/* ---- flyover hints -------------------------------------------------------- */
void rn_ffi_bridges_clear(void);
int32_t rn_ffi_bridges_add(const double *lat, const double *lon, int32_t n);
int32_t rn_ffi_bridges_count(void);
int32_t rn_ffi_flyover_annotate(void);
int32_t rn_ffi_flyover_by_name(void);

/* ---- corridor, map generation, map pack ----------------------------------- */
int32_t rn_ffi_corridor_plan(int32_t zoom, int32_t buffer_px);
int32_t rn_ffi_corridor_info(int32_t *origin_x, int32_t *origin_y, int32_t *tiles_w,
                             int32_t *tiles_h, int32_t *n_keep);
int32_t rn_ffi_corridor_keep(int32_t cx, int32_t cy);
/* Route vertex i in pack pixels (relative to the corridor origin). */
int32_t rn_ffi_corridor_point_px(int32_t i, float *x, float *y);
/* rgb: w*h*3 at zoom+1 (w, h even, <= 576); out: (w/2)*(h/2) classes. */
int32_t rn_ffi_mapgen_block(const uint8_t *rgb, int32_t w, int32_t h, uint8_t *out);
/* Tile style the next blocks come from: 0 OSM carto (default), 1 ROTOS's Google style. */
int32_t rn_ffi_mapgen_source(int32_t source);
int32_t rn_ffi_pack_begin(void);
int32_t rn_ffi_pack_add_tile(int32_t cx, int32_t cy, const uint8_t *classes);
int32_t rn_ffi_pack_finish(void);                 /* returns the pack length */
int32_t rn_ffi_pack_copy(uint8_t *out, int32_t cap);
/* The current pack compressed for the transfer (raw deflate, 4 KB window,
 * see rn_inflate.h); the map frames then carry these bytes. len 0 = raw.
 * Reset by every new pack. */
int32_t rn_ffi_wire_set(const uint8_t *z, int32_t len);
/* Bytes the map transfer sends (compressed or raw). */
int32_t rn_ffi_wire_len(void);
/* The current pack as a patch (rn_delta.h) against the pack with CRC
 * base_crc that ROTOS holds: rn_ffi_delta builds the patch, the phone
 * deflates it and hands it over here; MAP_BEGIN then names the base. */
int32_t rn_ffi_delta(const uint8_t *base, int32_t base_len, uint8_t *out, int32_t cap);
int32_t rn_ffi_wire_set_delta(const uint8_t *z, int32_t len, uint32_t base_crc);
uint32_t rn_ffi_pack_crc32(void);
uint32_t rn_ffi_pack_route_id(void);
/* Load an existing pack (e.g. from a file) as the current one. */
int32_t rn_ffi_pack_load(const uint8_t *blob, int32_t len);
/* Map class at pack pixel, -1 outside the corridor (preview rendering). */
int32_t rn_ffi_pack_class_at(int32_t x, int32_t y);

/* ---- navigation engine ---------------------------------------------------- */
void rn_ffi_engine_reset(void);
/* speed / heading < 0 = unknown. Returns the NAV_STATE frame length. */
int32_t rn_ffi_engine_fix(uint16_t seq, uint32_t t_ms, double unix_s, int32_t utc_offset_s,
                          double lat, double lon, float speed_mps, float heading_deg,
                          float accuracy_m, uint8_t *out, int32_t cap);
int32_t rn_ffi_engine_state(float *along_m, float *remaining_m, uint32_t *remaining_s,
                            uint8_t *flags, int32_t *wants_reroute);

/* ---- outbound frames (phone -> ROTOS) ------------------------------------- */
int32_t rn_ffi_frame_phone_status(uint16_t seq, uint32_t t_ms, uint32_t time_s, uint8_t weekday,
                                  uint8_t day, uint8_t month, uint8_t battery_pct, uint8_t flags,
                                  uint8_t signal_bars, uint8_t *out, int32_t cap);
/* PREP_STATUS: what the phone is doing before the map transfer (RN_PREP_*). */
int32_t rn_ffi_frame_prep_status(uint16_t seq, uint32_t t_ms, uint8_t stage, uint8_t pct,
                                 uint16_t done, uint16_t total, uint8_t *out, int32_t cap);
/* GUIDANCE: next turn from another navigation app (mirror mode). */
int32_t rn_ffi_frame_guidance(uint16_t seq, uint32_t t_ms, uint8_t maneuver, uint8_t flags,
                              uint16_t to_turn_m, uint32_t remaining_m, uint32_t eta_s, const char *road,
                              uint8_t *out, int32_t cap);
int32_t rn_ffi_frame_weather(uint16_t seq, uint32_t t_ms, float temp_c, uint8_t code,
                             uint8_t rain_prob, uint8_t wind_kmh, uint8_t is_day,
                             uint8_t *out, int32_t cap);
int32_t rn_ffi_frame_settings(uint16_t seq, uint8_t theme_mode, uint8_t north_up,
                              uint8_t glass_pct, uint8_t brightness_pct, uint8_t *out, int32_t cap);
int32_t rn_ffi_frame_ping(uint16_t seq, uint8_t *out, int32_t cap);
/* NAV_STATE with RN_NAV_FREE: a GPS position for ROTOS's map while another app
 * (Google Maps) navigates. route_id = the uploaded map pack's id; with
 * RN_NAV_LINE in flags the pack's route is drawn and along_m places the rider
 * on it. */
int32_t rn_ffi_frame_position(uint16_t seq, uint32_t t_ms, uint32_t route_id, double lat, double lon,
                              float speed_mps, float heading_deg, float accuracy_m, float along_m,
                              float remaining_m, uint32_t eta_s, uint8_t flags, uint8_t *out, int32_t cap);
/* Hourly forecast: n (<= 8) entries in parallel arrays. */
int32_t rn_ffi_frame_forecast(uint16_t seq, uint32_t t_ms, uint8_t start_hour, uint8_t n,
                              uint16_t sunrise_min, uint16_t sunset_min, const float *temp_c,
                              const uint8_t *code, const uint8_t *rain_prob, const uint8_t *wind_kmh,
                              uint8_t *out, int32_t cap);
/* Call state; name is UTF-8 or ASCII (reduced to upper-case ASCII, <= 24 chars). */
int32_t rn_ffi_frame_call_state(uint16_t seq, uint32_t t_ms, uint16_t call_id, uint8_t state,
                                uint8_t flags, const char *name, uint8_t *out, int32_t cap);
/* NOTIFY: one phone notification (app, sender, message; any text, cleaned to upper-case ASCII). */
int32_t rn_ffi_frame_notify(uint16_t seq, uint32_t t_ms, uint16_t id, uint8_t kind, const char *app,
                            const char *title, const char *text, uint8_t *out, int32_t cap);
/* Map transfer of the current pack. Chunk returns 0 past the end. */
int32_t rn_ffi_frame_map_begin(uint16_t seq, uint8_t *out, int32_t cap);
int32_t rn_ffi_frame_map_chunk(uint16_t seq, uint32_t offset, uint8_t *out, int32_t cap);
int32_t rn_ffi_frame_map_end(uint16_t seq, uint8_t *out, int32_t cap);
int32_t rn_ffi_map_chunk_size(void);

/* ---- inbound frames (ROTOS -> phone) -------------------------------------- */
void rn_ffi_rx_reset(void);
/* Feed received bytes; returns the number of complete packets queued. */
int32_t rn_ffi_rx_feed(const uint8_t *bytes, int32_t n);
/* Pop one packet: returns payload length (>= 0) or -1 when empty. */
int32_t rn_ffi_rx_next(uint8_t *type, uint16_t *seq, uint8_t *payload, int32_t cap);
int32_t rn_ffi_decode_ack(const uint8_t *payload, int32_t len, uint8_t *acked_type,
                          uint8_t *status, uint16_t *acked_seq, uint32_t *value);
int32_t rn_ffi_decode_button(const uint8_t *payload, int32_t len, uint8_t *button, uint8_t *event);
int32_t rn_ffi_decode_call_action(const uint8_t *payload, int32_t len, uint16_t *call_id, uint8_t *action);

/* ---- ride simulator ------------------------------------------------------- */
int32_t rn_ffi_sim_start(uint32_t seed, float cruise_mps);
int32_t rn_ffi_sim_step(float dt);                /* 1 = riding, 0 = arrived */
int32_t rn_ffi_sim_fix(float sigma_m, double *lat, double *lon, float *speed_mps,
                       float *heading_deg, float *accuracy_m);
float rn_ffi_sim_along(void);
float rn_ffi_sim_speed(void);
float rn_ffi_sim_time(void);

/* ---- misc ----------------------------------------------------------------- */
double rn_ffi_sun_elevation(double lat, double lon, double unix_s);
double rn_ffi_sun_next_sunset(double lat, double lon, double from_unix_s);
uint32_t rn_ffi_local_seconds(double unix_s, int32_t utc_offset_s);

#endif /* RN_FFI_H */
