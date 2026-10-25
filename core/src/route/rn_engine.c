/*
 * rn_engine.c - phone-side navigation engine.
 */
#include "rn/rn_engine.h"

#include <math.h>
#include <string.h>

#include "rn/rn_sun.h"

#define SPEED_TAU_S   1.5f    /* smoothing of along-derived speed */
#define GPS_OK_ACC_M 50.0f

void rn_engine_init(RnEngine *e)
{
    if (!RN_ASSERT(e != NULL)) {
        return;
    }
    (void)memset(e, 0, sizeof *e);
    rn_matcher_init(&e->matcher);
}

uint32_t rn_engine_local_seconds(double unix_s, int32_t utc_offset_s)
{
    (void)RN_ASSERT(isfinite(unix_s));
    double local = floor(unix_s) + (double)utc_offset_s;
    double s = fmod(local, 86400.0);
    if (s < 0.0) {
        s += 86400.0;
    }
    return (uint32_t)s % 86400u;
}

/* Speed: the GPS's Doppler speed when it has one, else from along deltas. */
static float update_speed(RnEngine *e, const RnFix *fix, float along)
{
    if (fix->speed_mps >= 0.0f) {
        e->speed_mps = fix->speed_mps;
    } else if (e->have_prev && fix->t_ms > e->prev_t_ms) {
        float dt = (float)(fix->t_ms - e->prev_t_ms) * 0.001f;
        float v = fmaxf((along - e->prev_along_m) / dt, 0.0f);
        float k = 1.0f - expf(-dt / SPEED_TAU_S);
        e->speed_mps += (v - e->speed_mps) * k;
    } else {
        e->speed_mps = 0.0f;
    }
    e->have_prev = true;
    e->prev_t_ms = fix->t_ms;
    e->prev_along_m = along;
    return e->speed_mps;
}

static uint8_t make_flags(RnEngine *e, const RnRoute *r, const RnFix *fix, float speed)
{
    uint8_t flags = 0u;
    if (fix->accuracy_m >= 0.0f && fix->accuracy_m < GPS_OK_ACC_M) {
        flags |= (uint8_t)RN_NAV_GPS_OK;
    }
    if (e->matcher.off_route) {
        flags |= (uint8_t)RN_NAV_OFF_ROUTE;
        if (e->off_since_ms == 0u) {
            e->off_since_ms = (fix->t_ms != 0u) ? fix->t_ms : 1u;
        }
    } else {
        e->off_since_ms = 0u;
    }
    if (rn_sun_is_night(fix->lat, fix->lon, fix->unix_s)) {
        flags |= (uint8_t)RN_NAV_NIGHT;
    }
    float remaining = r->total_m - e->matcher.along_m;
    if (!e->matcher.off_route && remaining < RN_ARRIVE_M && speed < RN_ARRIVE_MPS) {
        e->arrived = true;
    }
    if (e->arrived) {
        flags |= (uint8_t)RN_NAV_ARRIVED;
    }
    return flags;
}

rn_status rn_engine_on_fix(RnEngine *e, const RnRoute *r, const RnFix *fix, RnNavState *out)
{
    if (!RN_ASSERT(e != NULL && r != NULL && fix != NULL && out != NULL) ||
        !RN_ASSERT(r->total_m > 0.0f)) {
        return RN_ERR_PARAM;
    }
    RnMatchInput in;
    in.pos = rn_geo_to_local(&r->frame, fix->lat, fix->lon);
    in.speed_mps = fmaxf(fix->speed_mps, 0.0f);
    in.heading_deg = fix->heading_deg;
    in.heading_valid = fix->heading_deg >= 0.0f;
    in.accuracy_m = fmaxf(fix->accuracy_m, 0.0f);
    in.t_ms = fix->t_ms;
    rn_status st = rn_matcher_update(&e->matcher, r, &in);
    if (st != RN_OK) {
        return st;
    }
    float along = e->matcher.along_m;
    float speed = update_speed(e, fix, along);
    float remaining = fmaxf(r->total_m - along, 0.0f);
    float rem_s = r->duration_s * remaining / fmaxf(r->total_m, 1.0f);

    (void)memset(out, 0, sizeof *out);
    out->t_ms = fix->t_ms;
    out->route_id = r->route_id;
    out->lat = fix->lat;
    out->lon = fix->lon;
    out->speed_mps = speed;
    out->heading_deg = in.heading_valid ? fix->heading_deg
                                        : rn_route_heading_at(r, along) * (float)RN_RAD2DEG;
    out->along_m = along;
    out->remaining_m = remaining;
    out->remaining_s = (uint32_t)rem_s;
    out->eta_s = (rn_engine_local_seconds(fix->unix_s, fix->utc_offset_s) + (uint32_t)rem_s) % 86400u;
    out->flags = make_flags(e, r, fix, speed);
    out->accuracy_m = (uint8_t)fminf(fmaxf(fix->accuracy_m, 0.0f), 255.0f);
    return RN_OK;
}

bool rn_engine_wants_reroute(const RnEngine *e, uint32_t now_ms)
{
    if (!RN_ASSERT(e != NULL)) {
        return false;
    }
    return e->off_since_ms != 0u && now_ms - e->off_since_ms >= RN_REROUTE_AFTER_MS;
}
