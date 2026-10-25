/*
 * rt_phone.h - what ROTOS knows about the phone: clock, battery, signal,
 * weather, and whether the sun is down where it is.
 */
#ifndef RT_PHONE_H
#define RT_PHONE_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_protocol.h"
#include "rt_widgets.h"

typedef struct {
    bool have_status;
    RnPhoneStatus status;
    uint32_t status_rx_ms;
    bool have_weather;
    RnWeather weather;
    bool have_forecast;
    RnForecast forecast;
    uint32_t forecast_rx_ms;
    bool night;               /* latest NIGHT flag from NAV_STATE or STATUS */
    bool have_night;
    bool have_guide;          /* GUIDANCE: Google Maps navigating           */
    RnGuidance guide;
    uint32_t guide_rx_ms;
    uint32_t guide_dist_ms;   /* when Google's distance to the turn last changed */
    bool have_prep;           /* PREP_STATUS: phone preparing a route       */
    RnPrepStatus prep;
    uint32_t prep_rx_ms;
} RtPhone;

void rt_phone_init(RtPhone *p);
void rt_phone_on_status(RtPhone *p, const RnPhoneStatus *s, uint32_t now_ms);
void rt_phone_on_weather(RtPhone *p, const RnWeather *w);
void rt_phone_on_forecast(RtPhone *p, const RnForecast *f, uint32_t now_ms);
void rt_phone_on_guidance(RtPhone *p, const RnGuidance *g, uint32_t now_ms);
/* True while recent GUIDANCE says another app is navigating. */
bool rt_phone_guidance_active(const RtPhone *p, uint32_t now_ms);
void rt_phone_on_prep(RtPhone *p, const RnPrepStatus *s, uint32_t now_ms);
/* True while a recent PREP_STATUS says the phone is busy (stage not IDLE). */
bool rt_phone_prep_active(const RtPhone *p, uint32_t now_ms);
void rt_phone_on_night(RtPhone *p, bool night);
/* Local time of day now, advancing from the last status; false if unknown. */
bool rt_phone_clock(const RtPhone *p, uint32_t now_ms, uint32_t *time_s);
RtStatusView rt_phone_view(const RtPhone *p, bool link_ok);

#endif /* RT_PHONE_H */
