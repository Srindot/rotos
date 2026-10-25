/*
 * rt_phone.c - phone status bookkeeping.
 */
#include "rt_phone.h"

#include <string.h>

#include "rn/rn_types.h"
#include "rt_config.h"

void rt_phone_init(RtPhone *p)
{
    if (!RN_ASSERT(p != NULL)) {
        return;
    }
    (void)memset(p, 0, sizeof *p);
    p->night = true;                  /* boot dark: kind to eyes at night */
}

void rt_phone_on_status(RtPhone *p, const RnPhoneStatus *s, uint32_t now_ms)
{
    if (!RN_ASSERT(p != NULL && s != NULL)) {
        return;
    }
    p->status = *s;
    p->status_rx_ms = now_ms;
    p->have_status = true;
    rt_phone_on_night(p, (s->flags & RN_STATUS_NIGHT) != 0u);
}

void rt_phone_on_guidance(RtPhone *p, const RnGuidance *g, uint32_t now_ms)
{
    if (!RN_ASSERT(p != NULL && g != NULL)) {
        return;
    }
    if (!p->have_guide || g->to_turn_m != p->guide.to_turn_m || g->maneuver != p->guide.maneuver) {
        p->guide_dist_ms = now_ms;                /* a new distance: the countdown restarts here */
    }
    p->guide = *g;
    p->guide_rx_ms = now_ms;
    p->have_guide = (g->flags & RN_GUIDE_ACTIVE) != 0u;
}

bool rt_phone_guidance_active(const RtPhone *p, uint32_t now_ms)
{
    if (!RN_ASSERT(p != NULL)) {
        return false;
    }
    return p->have_guide && now_ms - p->guide_rx_ms < RT_GUIDE_TIMEOUT_MS;
}

void rt_phone_on_prep(RtPhone *p, const RnPrepStatus *s, uint32_t now_ms)
{
    if (!RN_ASSERT(p != NULL && s != NULL)) {
        return;
    }
    p->prep = *s;
    p->prep_rx_ms = now_ms;
    p->have_prep = true;
}

bool rt_phone_prep_active(const RtPhone *p, uint32_t now_ms)
{
    if (!RN_ASSERT(p != NULL)) {
        return false;
    }
    return p->have_prep && p->prep.stage != RN_PREP_IDLE && now_ms - p->prep_rx_ms < RT_PREP_TIMEOUT_MS;
}

void rt_phone_on_weather(RtPhone *p, const RnWeather *w)
{
    if (!RN_ASSERT(p != NULL && w != NULL)) {
        return;
    }
    p->weather = *w;
    p->have_weather = true;
}

void rt_phone_on_forecast(RtPhone *p, const RnForecast *f, uint32_t now_ms)
{
    if (!RN_ASSERT(p != NULL && f != NULL) || !RN_ASSERT(f->n <= RN_FORECAST_HOURS)) {
        return;
    }
    p->forecast = *f;
    p->forecast_rx_ms = now_ms;
    p->have_forecast = true;
}

void rt_phone_on_night(RtPhone *p, bool night)
{
    if (!RN_ASSERT(p != NULL)) {
        return;
    }
    p->night = night;
    p->have_night = true;
}

bool rt_phone_clock(const RtPhone *p, uint32_t now_ms, uint32_t *time_s)
{
    if (!RN_ASSERT(p != NULL && time_s != NULL) || !p->have_status) {
        return false;
    }
    *time_s = (p->status.time_s + (now_ms - p->status_rx_ms) / 1000u) % 86400u;
    return true;
}

RtStatusView rt_phone_view(const RtPhone *p, bool link_ok)
{
    RtStatusView v = {false, false, 0u, false, 0u, false};
    if (!RN_ASSERT(p != NULL)) {
        return v;
    }
    v.known = p->have_status;
    v.link_ok = link_ok;
    v.battery = p->status.battery_pct;
    v.charging = (p->status.flags & RN_STATUS_CHARGING) != 0u;
    v.bars = p->status.signal_bars;
    v.google_cap = (p->status.flags & RN_STATUS_GOOGLE_CAP) != 0u;
    return v;
}
