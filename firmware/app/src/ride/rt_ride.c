/*
 * rt_ride.c - ride statistics.
 */
#include "rt_ride.h"

#include <string.h>

#include "rn/rn_types.h"
#include "rt_config.h"

#define MAX_STEP_M 50.0f     /* bigger along jumps are reroutes, not riding */

void rt_ride_start(RtRide *r, uint32_t now_ms, bool have_clock, uint32_t clock_s)
{
    if (!RN_ASSERT(r != NULL)) {
        return;
    }
    (void)memset(r, 0, sizeof *r);
    r->active = true;
    r->start_ms = now_ms;
    r->have_clock = have_clock;
    r->start_s = clock_s;
}

void rt_ride_restore(RtRide *r, uint32_t now_ms, float distance_m, uint32_t ride_ms, uint32_t moving_ms,
                     float max_mps, bool have_clock, uint32_t start_s)
{
    if (!RN_ASSERT(r != NULL)) {
        return;
    }
    rt_ride_start(r, now_ms - ride_ms, have_clock, start_s);
    r->distance_m = (distance_m > 0.0f) ? distance_m : 0.0f;
    r->moving_ms = moving_ms;
    r->max_mps = (max_mps > 0.0f) ? max_mps : 0.0f;
}

void rt_ride_rebase(RtRide *r)
{
    if (!RN_ASSERT(r != NULL)) {
        return;
    }
    r->have_last = false;
}

void rt_ride_update(RtRide *r, float along_m, float speed_mps, uint32_t now_ms)
{
    if (!RN_ASSERT(r != NULL) || !r->active) {
        return;
    }
    if (r->have_last) {
        float d = along_m - r->last_along;
        if (d > 0.0f && d < MAX_STEP_M) {
            r->distance_m += d;
        }
        if (speed_mps > RT_MOVING_MPS && d > 0.0f) {
            r->moving_ms += now_ms - r->last_ms;
        }
    }
    r->max_mps = (speed_mps > r->max_mps) ? speed_mps : r->max_mps;
    r->last_along = along_m;
    r->last_ms = now_ms;
    r->have_last = true;
}

RtRideSummary rt_ride_finish(RtRide *r, uint32_t now_ms, bool have_clock, uint32_t clock_s)
{
    RtRideSummary s;
    (void)memset(&s, 0, sizeof s);
    if (!RN_ASSERT(r != NULL)) {
        return s;
    }
    r->active = false;
    s.distance_m = r->distance_m;
    s.ride_s = (now_ms - r->start_ms) / 1000u;
    s.moving_s = r->moving_ms / 1000u;
    s.avg_kmh = (s.moving_s > 0u) ? r->distance_m / (float)s.moving_s * 3.6f : 0.0f;
    s.max_kmh = r->max_mps * 3.6f;
    s.have_clock = r->have_clock && have_clock;
    s.start_s = r->start_s;
    s.end_s = clock_s;
    return s;
}
