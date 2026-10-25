/*
 * rn_tracker.c - device-side smoothing and dead reckoning.
 */
#include "rn/rn_tracker.h"

#include <math.h>
#include <string.h>

#include "rn/rn_geo.h"

#define MAX_STEP_S 0.25f

void rn_tracker_reset(RnTracker *t)
{
    if (!RN_ASSERT(t != NULL)) {
        return;
    }
    (void)memset(t, 0, sizeof *t);
}

void rn_tracker_on_nav(RnTracker *t, const RnNavState *s, uint32_t now_ms)
{
    if (!RN_ASSERT(t != NULL && s != NULL)) {
        return;
    }
    t->last = *s;
    t->rx_ms = now_ms;
    t->have = true;
}

uint32_t rn_tracker_age_ms(const RnTracker *t, uint32_t now_ms)
{
    if (!RN_ASSERT(t != NULL) || !t->have) {
        return UINT32_MAX;
    }
    return now_ms - t->rx_ms;
}

static float ease(float dt, float tau)
{
    return 1.0f - expf(-dt / tau);
}

static void update_along(RnTracker *t, float pred, float dt)
{
    if (!t->init) {
        t->along_m = pred;
        t->speed_mps = t->last.speed_mps;
        t->init = true;
        return;
    }
    float diff = pred - t->along_m;
    if (fabsf(diff) > RN_TRACK_JUMP_M) {
        t->along_m = pred;                       /* reroute or long gap */
    } else if (diff > 0.0f || diff < -RN_TRACK_BACK_M) {
        t->along_m += diff * ease(dt, RN_TRACK_ALONG_TAU_S);
    } else {
        /* small backward correction: hold, never jitter backwards */
    }
    t->speed_mps += (t->last.speed_mps - t->speed_mps) * ease(dt, RN_TRACK_SPEED_TAU_S);
}

static void update_heading(RnTracker *t, const RnPack *pack, float dt)
{
    RnPackPoint a = rn_pack_position_at(pack, t->along_m - 5.0f);
    RnPackPoint b = rn_pack_position_at(pack, t->along_m + 25.0f);
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    if (dx * dx + dy * dy <= 4.0f) {
        return;                                  /* too short to tell */
    }
    float target = atan2f(dx, -dy);              /* pack y grows southwards */
    if (dt <= 0.0f) {
        t->heading_rad = target;
        return;
    }
    float d = rn_geo_wrap_pi(target - t->heading_rad);
    t->heading_rad = rn_geo_wrap_pi(t->heading_rad + d * ease(dt, RN_TRACK_HEADING_TAU_S));
}

/* RN_NAV_FREE: GPS position -> pack pixels, dead-reckoned along the GPS
 * course, eased; heading follows the course while moving. */
static void update_free(RnTracker *t, const RnPack *pack, float dt, float age)
{
    double wx = 0.0;
    double wy = 0.0;
    rn_geo_world_px(t->last.lat, t->last.lon, pack->h.zoom, &wx, &wy);
    bool has_course = (t->last.flags & RN_NAV_HEADING) != 0u;
    float course = has_course ? t->last.heading_deg * (RN_PI_F / 180.0f) : t->heading_rad;
    float ahead = has_course ? t->last.speed_mps * age / ((pack->h.m_per_px > 0.0f) ? pack->h.m_per_px : 1.0f) : 0.0f;
    float tx = (float)(wx - (double)pack->h.origin_x) + sinf(course) * ahead;
    float ty = (float)(wy - (double)pack->h.origin_y) - cosf(course) * ahead;
    float jump_px = RN_TRACK_JUMP_M / ((pack->h.m_per_px > 0.0f) ? pack->h.m_per_px : 1.0f);
    float dx = tx - t->x_px;
    float dy = ty - t->y_px;
    if (!t->init || !t->free || dx * dx + dy * dy > jump_px * jump_px) {
        t->x_px = tx;
        t->y_px = ty;
        t->heading_rad = rn_geo_wrap_pi(course);
        t->speed_mps = t->last.speed_mps;
        t->along_m = t->last.along_m;
        t->init = true;
        t->free = true;
        return;
    }
    float k = ease(dt, RN_TRACK_FREE_TAU_S);
    t->x_px += dx * k;
    t->y_px += dy * k;
    if ((t->last.flags & RN_NAV_LINE) != 0u) {           /* along the drawn line (may be a run-time ROUTE, longer than the pack's) */
        float pred = t->last.along_m + t->last.speed_mps * age;
        t->along_m += (pred - t->along_m) * ease(dt, RN_TRACK_ALONG_TAU_S);
    }
    if (t->last.speed_mps >= RN_TRACK_TURN_MPS) {          /* not GPS jitter at a stop */
        t->odo_m += sqrtf(dx * dx + dy * dy) * k * pack->h.m_per_px;
    }
    t->speed_mps += (t->last.speed_mps - t->speed_mps) * ease(dt, RN_TRACK_SPEED_TAU_S);
    if (has_course && t->last.speed_mps >= RN_TRACK_TURN_MPS) {
        float d = rn_geo_wrap_pi(course - t->heading_rad);
        t->heading_rad = rn_geo_wrap_pi(t->heading_rad + d * ease(dt, RN_TRACK_HEADING_TAU_S));
    }
}

void rn_tracker_update(RnTracker *t, const RnPack *pack, uint32_t now_ms)
{
    if (!RN_ASSERT(t != NULL && pack != NULL) || !t->have) {
        return;
    }
    float dt = t->init ? fminf((float)(now_ms - t->prev_ms) * 0.001f, MAX_STEP_S) : 0.0f;
    t->prev_ms = now_ms;
    float age = fminf((float)(now_ms - t->rx_ms) * 0.001f, RN_TRACK_MAX_PREDICT_S);
    if ((t->last.flags & RN_NAV_FREE) != 0u) {
        update_free(t, pack, dt, age);
        return;
    }
    t->free = false;
    float pred = fminf(t->last.along_m + t->last.speed_mps * age, pack->h.total_m);
    update_along(t, pred, dt);
    update_heading(t, pack, dt);
}
