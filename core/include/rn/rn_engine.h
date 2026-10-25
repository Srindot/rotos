/*
 * rn_engine.h - the phone's navigation engine: GPS fix in, NAV_STATE out.
 *
 * Runs on the phone (via JNI) and in the simulator's virtual phone (via
 * ctypes): map matching, remaining distance, ETA, off-route and arrival
 * detection, and the day/night flag from the sun's position.
 */
#ifndef RN_ENGINE_H
#define RN_ENGINE_H

#include "rn/rn_matcher.h"
#include "rn/rn_protocol.h"
#include "rn/rn_route.h"

#define RN_ARRIVE_M          15.0f   /* within this of the end ...         */
#define RN_ARRIVE_MPS         1.5f   /* ... and slower than this = arrived */
#define RN_REROUTE_AFTER_MS  8000u   /* off route this long: ask for a new route */

typedef struct {
    uint32_t t_ms;          /* phone monotonic clock                     */
    double unix_s;          /* wall clock, UTC (sun position)            */
    int32_t utc_offset_s;   /* local time = UTC + offset (ETA display)   */
    double lat;
    double lon;
    float speed_mps;        /* < 0 when the GPS has no speed             */
    float heading_deg;      /* < 0 when the GPS has no course            */
    float accuracy_m;
} RnFix;

typedef struct {
    RnMatcher matcher;
    bool have_prev;
    uint32_t prev_t_ms;
    float prev_along_m;
    float speed_mps;        /* smoothed                                  */
    uint32_t off_since_ms;  /* 0 = on route                              */
    bool arrived;
} RnEngine;

void rn_engine_init(RnEngine *e);

/* Process one fix; fills *out (route_id, flags, ETA...). */
rn_status rn_engine_on_fix(RnEngine *e, const RnRoute *r, const RnFix *fix, RnNavState *out);

/* True when the rider has been off route long enough to request a reroute. */
bool rn_engine_wants_reroute(const RnEngine *e, uint32_t now_ms);

/* Local seconds of the day for a UTC time and offset. */
uint32_t rn_engine_local_seconds(double unix_s, int32_t utc_offset_s);

#endif /* RN_ENGINE_H */
