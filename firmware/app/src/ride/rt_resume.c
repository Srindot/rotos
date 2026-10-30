/*
 * rt_resume.c - the ride in progress across power cuts.
 */
#include "rt_resume.h"

#include <string.h>

#include "hal_storage.h"
#include "rn/rn_types.h"

#define DAY_S 86400u

RtResumeSnap rt_resume_snap(const RtRide *ride, const RnNavState *nav, uint32_t now_ms,
                            uint32_t saved_s, uint8_t day, uint8_t month)
{
    RtResumeSnap s;
    (void)memset(&s, 0, sizeof s);
    if (!RN_ASSERT(ride != NULL && nav != NULL)) {
        return s;
    }
    s.version = (uint8_t)RT_RESUME_VERSION;
    s.have_clock = ride->have_clock ? 1u : 0u;
    s.saved_day = day;
    s.saved_month = month;
    s.saved_s = saved_s % DAY_S;
    s.start_s = ride->start_s;
    s.ride_ms = now_ms - ride->start_ms;
    s.moving_ms = ride->moving_ms;
    s.distance_m = ride->distance_m;
    s.max_mps = ride->max_mps;
    s.nav = *nav;
    s.nav.speed_mps = 0.0f;                   /* restored standing still: no running ahead */
    return s;
}

bool rt_resume_save(const RtResumeSnap *s)
{
    return RN_ASSERT(s != NULL) && hal_storage_ride_save((const uint8_t *)s, (uint32_t)sizeof *s);
}

bool rt_resume_load(RtResumeSnap *out)
{
    if (!RN_ASSERT(out != NULL) || !hal_storage_ride_load((uint8_t *)out, (uint32_t)sizeof *out)) {
        return false;
    }
    return out->version == (uint8_t)RT_RESUME_VERSION && out->saved_s < DAY_S;
}

void rt_resume_clear(void)
{
    hal_storage_ride_clear();
}

uint32_t rt_resume_off_s(const RtResumeSnap *s, uint32_t now_s, uint8_t day, uint8_t month)
{
    if (!RN_ASSERT(s != NULL)) {
        return RT_RESUME_OFF_UNKNOWN;
    }
    uint32_t now = now_s % DAY_S;
    bool same_day = day == s->saved_day && month == s->saved_month;
    if (same_day) {
        return (now >= s->saved_s) ? now - s->saved_s : RT_RESUME_OFF_UNKNOWN;
    }
    /* just past midnight: the next day, earlier in the day than the save */
    bool next_day = (day == (uint8_t)(s->saved_day + 1u) && month == s->saved_month) ||
                    (day == 1u && month != s->saved_month);
    return (next_day && now < s->saved_s) ? now + DAY_S - s->saved_s : RT_RESUME_OFF_UNKNOWN;
}
