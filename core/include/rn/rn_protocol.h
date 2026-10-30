/*
 * rn_protocol.h - the phone <-> ROTOS link protocol (version 2).
 *
 * Frame (all fields little-endian):
 *
 *   | A5 5A | VERSION u8 | TYPE u8 | SEQ u16 | LEN u16 | PAYLOAD[LEN] | CRC16 u16 |
 *
 * CRC-16/CCITT (init 0xFFFF) over VERSION..PAYLOAD. The byte stream may be cut
 * anywhere (BLE writes, TCP segments, capture files); the parser resyncs on
 * garbage. One SEQ counter per direction; gaps count as lost packets.
 *
 * Phone -> ROTOS                         ROTOS -> phone
 *   0x01 NAV_STATE      ~1 Hz              0x21 ACK     map transfer flow
 *   0x02 PHONE_STATUS   every 5 s          0x22 BUTTON  button events
 *   0x03 WEATHER        every 10 min       0x20 PING    keep-alive (both ways)
 *   0x04 SETTINGS       on change
 *   0x05 FORECAST       every 10 min       0x23 CALL_ACTION  call popup buttons
 *   0x06 CALL_STATE     on call changes
 *   0x07 PREP_STATUS    ~1 Hz while the phone prepares a route
 *   0x08 GUIDANCE       ~1 Hz while Google Maps navigates (mirror mode)
 *   0x09 NOTIFY         a new phone notification (messages, apps)
 *   0x0A ROUTE          a new route line for the current map (reroutes)
 *   0x10 MAP_BEGIN \
 *   0x11 MAP_CHUNK  >   map pack transfer at route start / reroute
 *   0x12 MAP_END   /
 *
 * Payload layouts are documented next to each struct and in docs/PROTOCOL.md.
 */
#ifndef RN_PROTOCOL_H
#define RN_PROTOCOL_H

#include "rn/rn_types.h"

#define RN_PROTO_VERSION     2u
#define RN_SYNC0             0xA5u
#define RN_SYNC1             0x5Au
#define RN_PKT_HEADER        8u      /* sync(2) version type seq(2) len(2)  */
#define RN_PKT_TRAILER       2u      /* crc16                                */
#define RN_PKT_MAX_PAYLOAD   248u
#define RN_PKT_MAX_FRAME     (RN_PKT_HEADER + RN_PKT_MAX_PAYLOAD + RN_PKT_TRAILER)
#define RN_MAP_CHUNK_DATA    240u    /* map bytes per MAP_CHUNK              */

/* Packet types */
#define RN_PKT_NAV_STATE     0x01u
#define RN_PKT_PHONE_STATUS  0x02u
#define RN_PKT_WEATHER       0x03u
#define RN_PKT_SETTINGS      0x04u
#define RN_PKT_FORECAST      0x05u
#define RN_PKT_CALL_STATE    0x06u
#define RN_PKT_PREP_STATUS   0x07u
#define RN_PKT_GUIDANCE      0x08u
#define RN_PKT_NOTIFY        0x09u
#define RN_PKT_ROUTE         0x0Au
#define RN_PKT_MAP_BEGIN     0x10u
#define RN_PKT_MAP_CHUNK     0x11u
#define RN_PKT_MAP_END       0x12u
#define RN_PKT_PING          0x20u
#define RN_PKT_ACK           0x21u
#define RN_PKT_BUTTON        0x22u
#define RN_PKT_CALL_ACTION   0x23u
#define RN_PKT_DEVICE_STATUS 0x24u      /* ROTOS -> phone: uptime, why it (re)started */

/* NAV_STATE flags */
#define RN_NAV_GPS_OK        0x01u
#define RN_NAV_OFF_ROUTE     0x02u
#define RN_NAV_REROUTING     0x04u
#define RN_NAV_NIGHT         0x08u
#define RN_NAV_ARRIVED       0x10u
#define RN_NAV_FREE          0x20u   /* position from lat/lon, not along a
                                        ROTOS route (Google Maps navigates) */
#define RN_NAV_HEADING       0x80u   /* with FREE: heading_deg is a real GPS
                                        course (else hold heading, no dead reckoning) */
#define RN_NAV_LINE          0x40u   /* with FREE: the pack's route is Google's,
                                        draw it; along_m is valid            */

/* PHONE_STATUS flags */
#define RN_STATUS_CHARGING   0x01u
#define RN_STATUS_NIGHT      0x02u
#define RN_STATUS_GOOGLE_CAP 0x04u   /* phone near / at its Google usage cap:
                                        maps may come from OpenStreetMap      */

/* ACK status */
#define RN_ACK_OK            0u
#define RN_ACK_BAD_CRC       1u
#define RN_ACK_TOO_BIG       2u
#define RN_ACK_BAD_STATE     3u
#define RN_ACK_STORAGE       4u
#define RN_ACK_HAVE          5u      /* MAP_BEGIN: that exact pack is already stored */
#define RN_ACK_NO_BASE       6u      /* MAP_BEGIN: patch against a pack ROTOS does not hold */

/* SETTINGS theme modes */
#define RN_THEME_AUTO        0u
#define RN_THEME_DAY         1u
#define RN_THEME_NIGHT       2u

/* CALL_STATE states and flags */
#define RN_CALL_IDLE         0u      /* no call / call ended                */
#define RN_CALL_RINGING      1u
#define RN_CALL_ACTIVE       2u      /* answered on the phone               */
#define RN_CALL_F_REPLIED    0x01u   /* rejected and texted back            */
#define RN_CALL_F_MISSED     0x02u
#define RN_CALL_NAME_MAX     24u

/* CALL_ACTION actions */
#define RN_ACTION_REJECT_REPLY 1u
#define RN_ACTION_DISMISS      2u

/* FORECAST */
#define RN_FORECAST_HOURS    8u

/* BUTTON events */
#define RN_BUTTON_SHORT      1u
#define RN_BUTTON_LONG       2u

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t seq;
    uint16_t len;
    uint8_t payload[RN_PKT_MAX_PAYLOAD];
} RnPacket;

/* NAV_STATE, 38 bytes: u32 t_ms, u32 route_id, i32 lat*1e7, i32 lon*1e7,
 * u16 speed cm/s, u16 heading 0.01 deg, f32 along_m, f32 remaining_m,
 * u32 remaining_s, u32 eta_s (local seconds of day), u8 flags, u8 accuracy_m */
typedef struct {
    uint32_t t_ms;
    uint32_t route_id;
    double lat;
    double lon;
    float speed_mps;
    float heading_deg;
    float along_m;
    float remaining_m;
    uint32_t remaining_s;
    uint32_t eta_s;
    uint8_t flags;
    uint8_t accuracy_m;
} RnNavState;

/* PHONE_STATUS, 14 bytes: u32 t_ms, u32 time_s (local seconds of day),
 * u8 weekday (0 = Monday), u8 day, u8 month, u8 battery_pct, u8 flags,
 * u8 signal_bars (0..4) */
typedef struct {
    uint32_t t_ms;
    uint32_t time_s;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    uint8_t battery_pct;
    uint8_t flags;
    uint8_t signal_bars;
} RnPhoneStatus;

/* WEATHER, 10 bytes: u32 t_ms, i16 temp 0.1 C, u8 WMO code, u8 rain %,
 * u8 wind km/h, u8 is_day */
typedef struct {
    uint32_t t_ms;
    float temp_c;
    uint8_t code;
    uint8_t rain_prob;
    uint8_t wind_kmh;
    bool is_day;
} RnWeather;

/* SETTINGS, 4 bytes: u8 theme_mode, u8 north_up, u8 glass_pct,
 * u8 brightness_pct (0 = automatic from the light sensor) */
typedef struct {
    uint8_t theme_mode;
    uint8_t north_up;
    uint8_t glass_pct;
    uint8_t brightness_pct;
} RnSettings;

/* FORECAST, 10 + 5n bytes: u32 t_ms, u8 start_hour (local hour of hours[0]),
 * u8 n (<= 8), u16 sunrise (local minutes of day), u16 sunset, then n x
 * (i16 temp 0.1 C, u8 WMO code, u8 rain %, u8 wind km/h) */
typedef struct {
    float temp_c;
    uint8_t code;
    uint8_t rain_prob;
    uint8_t wind_kmh;
} RnForecastHour;

typedef struct {
    uint32_t t_ms;
    uint8_t start_hour;
    uint8_t n;
    uint16_t sunrise_min;
    uint16_t sunset_min;
    RnForecastHour hours[RN_FORECAST_HOURS];
} RnForecast;

/* CALL_STATE, 32 bytes: u32 t_ms, u16 call_id, u8 state, u8 flags,
 * char name[24] (caller name or number, upper-case ASCII, NUL-padded) */
typedef struct {
    uint32_t t_ms;
    uint16_t call_id;
    uint8_t state;
    uint8_t flags;
    char name[RN_CALL_NAME_MAX + 1u];
} RnCallState;

/* NOTIFY, 168 bytes: u32 t_ms, u16 id, u8 kind, u8 reserved, char app[16],
 * char title[24], char text[120] (upper-case ASCII, NUL-padded). One
 * notification as the phone shows it: app name, sender, message. */
#define RN_NOTIFY_APP_MAX    16u
#define RN_NOTIFY_TITLE_MAX  24u
#define RN_NOTIFY_TEXT_MAX  120u
#define RN_NOTIFY_MESSAGE     0u     /* chat / SMS                          */
#define RN_NOTIFY_OTHER       1u
typedef struct {
    uint32_t t_ms;
    uint16_t id;
    uint8_t kind;
    char app[RN_NOTIFY_APP_MAX + 1u];
    char title[RN_NOTIFY_TITLE_MAX + 1u];
    char text[RN_NOTIFY_TEXT_MAX + 1u];
} RnNotify;

/* ROUTE, 17 + 12n bytes: u32 route_id, u32 pack_route_id, u16 total, u16 first,
 * u8 n (1..18), f32 total_m, then n x (f32 x, f32 y, f32 along) in the pack
 * pixels of the pack whose route id is pack_route_id. One line (route_id)
 * arrives as ceil(total / 18) chunks in any order; ROTOS answers
 * ACK(ROUTE, OK, value = route_id) once it has every point. */
#define RN_ROUTE_CHUNK_PTS   18u
#define RN_ROUTE_MAX_PTS   1000u        /* = RN_LINE_MAX_PTS */
typedef struct {
    uint32_t route_id;
    uint32_t pack_route_id;
    uint16_t total;
    uint16_t first;
    uint8_t n;
    float total_m;
    float x[RN_ROUTE_CHUNK_PTS];
    float y[RN_ROUTE_CHUNK_PTS];
    float along[RN_ROUTE_CHUNK_PTS];
} RnRouteChunk;

/* PREP_STATUS stages: what the phone is doing before the map transfer. */
#define RN_PREP_IDLE         0u      /* nothing (clears the panel)          */
#define RN_PREP_ROUTE        1u      /* asking the routing service          */
#define RN_PREP_BRIDGES      2u      /* flyover / bridge data               */
#define RN_PREP_MAP          3u      /* map tiles + map pack, done / total  */
#define RN_PREP_READY        4u      /* map pack built, transfer next       */
#define RN_PREP_FAILED       5u      /* planning failed (see the phone)     */
#define RN_PREP_STAGE_MAX    RN_PREP_FAILED

/* PREP_STATUS, 10 bytes: u32 t_ms, u8 stage, u8 pct (0..100, whole job),
 * u16 done, u16 total (units of the stage, e.g. map blocks; 0 = none) */
typedef struct {
    uint32_t t_ms;
    uint8_t stage;
    uint8_t pct;
    uint16_t done;
    uint16_t total;
} RnPrepStatus;

/* GUIDANCE flags */
#define RN_GUIDE_ACTIVE      0x01u   /* clear = guidance ended (hide it)    */
#define RN_GUIDE_ARRIVED     0x02u
#define RN_GUIDE_NIGHT       0x04u
#define RN_GUIDE_ROAD_MAX    32u
#define RN_GUIDE_UNKNOWN16   0xFFFFu
#define RN_GUIDE_UNKNOWN32   0xFFFFFFFFu

/* GUIDANCE, 48 bytes: turn-by-turn from another navigation app (Google Maps,
 * read from its navigation notification), shown without a map pack.
 * u32 t_ms, u8 maneuver (RnManeuverType), u8 flags, u16 to_turn_m,
 * u32 remaining_m, u32 eta_s (local seconds of day), char road[32]
 * (upper-case ASCII, NUL-padded). Unknown numbers are all-ones. */
typedef struct {
    uint32_t t_ms;
    uint8_t maneuver;
    uint8_t flags;
    uint16_t to_turn_m;
    uint32_t remaining_m;
    uint32_t eta_s;
    char road[RN_GUIDE_ROAD_MAX + 1u];
} RnGuidance;

/* CALL_ACTION, 4 bytes: u16 call_id, u8 action, u8 reserved */
typedef struct {
    uint16_t call_id;
    uint8_t action;
} RnCallAction;

/* MAP_BEGIN, 20 bytes: u32 total_len, u32 crc32, u32 route_id, u32 wire_len, u32 base_crc.
 * total_len / crc32 describe the pack as stored. wire_len = 0: the chunks
 * carry the pack itself; else they carry wire_len bytes of raw deflate
 * (4 KB window, rn_inflate.h) that inflate to the pack. Chunk offsets and
 * ACK values count wire bytes. base_crc != 0: the inflated stream is a patch
 * (rn_delta.h) against the pack with that CRC, which ROTOS must hold (else it
 * answers RN_ACK_NO_BASE and the phone sends the whole pack). 12- and
 * 16-byte MAP_BEGINs still decode (wire_len / base_crc 0). */
typedef struct {
    uint32_t total_len;
    uint32_t crc32;
    uint32_t route_id;
    uint32_t wire_len;
    uint32_t base_crc;
} RnMapBegin;

/* MAP_CHUNK, 6 + n bytes: u32 offset, u16 n, u8 data[n] (n <= 240) */
typedef struct {
    uint32_t offset;
    uint16_t n;
    uint8_t data[RN_MAP_CHUNK_DATA];
} RnMapChunk;

/* MAP_END, 8 bytes: u32 total_len, u32 crc32 */
typedef struct {
    uint32_t total_len;
    uint32_t crc32;
} RnMapEnd;

/* ACK, 8 bytes: u8 acked_type, u8 status, u16 acked_seq, u32 value
 * (bytes received so far for map transfers) */
typedef struct {
    uint8_t acked_type;
    uint8_t status;
    uint16_t acked_seq;
    uint32_t value;
} RnAck;

/* DEVICE_STATUS, 12 bytes (ROTOS -> phone, on every link-up and when a ride
 * resume is decided): u32 uptime_ms, u8 reset_reason (esp_reset_reason_t:
 * 1 power on, 9 brownout ...), u8 flags, u16 reserved, u32 off_s (how long
 * ROTOS was off, from the phone's clock; RN_DEVICE_OFF_UNKNOWN until known) */
#define RN_DEVICE_RIDE_FOUND   0x01u  /* a ride was in progress when power went */
#define RN_DEVICE_RIDE_RESUMED 0x02u  /* off briefly: the same ride goes on      */
#define RN_DEVICE_RIDE_CLOSED  0x04u  /* off too long: that ride was closed      */
#define RN_DEVICE_OFF_UNKNOWN  0xFFFFFFFFu
typedef struct {
    uint32_t uptime_ms;
    uint8_t reset_reason;
    uint8_t flags;
    uint32_t off_s;
} RnDeviceStatus;

/* BUTTON, 2 bytes: u8 button (0..2), u8 event */
typedef struct {
    uint8_t button;
    uint8_t event;
} RnButton;

/* ---- framing -------------------------------------------------------------- */

/* Encode a full frame into out. Returns RN_OK and sets *out_len. */
rn_status rn_frame_encode(uint8_t type, uint16_t seq, const uint8_t *payload,
                          uint16_t len, uint8_t *out, uint32_t cap,
                          uint32_t *out_len);

/* ---- payload codecs (payload bytes only, no framing) ---------------------- */
rn_status rn_nav_state_encode(const RnNavState *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_nav_state_decode(const RnPacket *p, RnNavState *out);
rn_status rn_phone_status_encode(const RnPhoneStatus *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_phone_status_decode(const RnPacket *p, RnPhoneStatus *out);
rn_status rn_weather_encode(const RnWeather *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_weather_decode(const RnPacket *p, RnWeather *out);
rn_status rn_settings_encode(const RnSettings *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_settings_decode(const RnPacket *p, RnSettings *out);
rn_status rn_forecast_encode(const RnForecast *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_forecast_decode(const RnPacket *p, RnForecast *out);
rn_status rn_call_state_encode(const RnCallState *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_call_state_decode(const RnPacket *p, RnCallState *out);
rn_status rn_route_chunk_encode(const RnRouteChunk *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_route_chunk_decode(const RnPacket *p, RnRouteChunk *out);
rn_status rn_notify_encode(const RnNotify *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_notify_decode(const RnPacket *p, RnNotify *out);
rn_status rn_prep_status_encode(const RnPrepStatus *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_prep_status_decode(const RnPacket *p, RnPrepStatus *out);
rn_status rn_guidance_encode(const RnGuidance *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_guidance_decode(const RnPacket *p, RnGuidance *out);
rn_status rn_call_action_encode(const RnCallAction *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_call_action_decode(const RnPacket *p, RnCallAction *out);
rn_status rn_map_begin_encode(const RnMapBegin *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_map_begin_decode(const RnPacket *p, RnMapBegin *out);
rn_status rn_map_chunk_encode(const RnMapChunk *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_map_chunk_decode(const RnPacket *p, RnMapChunk *out);
rn_status rn_map_end_encode(const RnMapEnd *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_map_end_decode(const RnPacket *p, RnMapEnd *out);
rn_status rn_device_status_encode(const RnDeviceStatus *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_device_status_decode(const RnPacket *p, RnDeviceStatus *out);
rn_status rn_ack_encode(const RnAck *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_ack_decode(const RnPacket *p, RnAck *out);
rn_status rn_button_encode(const RnButton *s, uint8_t *out, uint32_t cap, uint16_t *len);
rn_status rn_button_decode(const RnPacket *p, RnButton *out);

/* ---- stream parser -------------------------------------------------------- */

typedef enum {
    RN_PARSE_MORE = 0,     /* byte consumed, no packet yet          */
    RN_PARSE_PACKET = 1,   /* *out holds a complete, valid packet   */
    RN_PARSE_ERROR = 2     /* a frame was rejected (crc or length)  */
} RnParseResult;

typedef struct {
    uint32_t have;
    uint8_t buf[RN_PKT_MAX_FRAME];
    int32_t last_seq;      /* -1 before the first packet               */
    uint32_t ok;           /* statistics                               */
    uint32_t crc_errors;
    uint32_t dropped;
} RnParser;

void rn_parser_init(RnParser *p);
/* Push one byte; when RN_PARSE_PACKET is returned, *out is filled. */
RnParseResult rn_parser_push(RnParser *p, uint8_t byte, RnPacket *out);
/* After a resync several complete frames can be buffered; call until it
 * stops returning RN_PARSE_PACKET after feeding a chunk of bytes. */
RnParseResult rn_parser_drain(RnParser *p, RnPacket *out);

#endif /* RN_PROTOCOL_H */
