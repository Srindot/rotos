/*
 * rt_screens.h - full-screen layouts. Each takes plain data (no app state):
 * what to show is decided in rt_app, how it looks is decided here.
 */
#ifndef RT_SCREENS_H
#define RT_SCREENS_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_protocol.h"
#include "rt_canvas.h"
#include "rt_theme.h"
#include "rt_widgets.h"

typedef struct {
    uint8_t next;            /* RnManeuverType; RN_M_NONE when nothing ahead */
    float next_dist_m;
    const char *next_road;
    uint8_t then;            /* follow-up if it comes quickly, else RN_M_NONE */
    const char *current_road;
    int32_t speed_kmh;
    uint32_t eta_s;          /* local seconds of day                         */
    float remaining_m;
    uint8_t flyover;         /* RnFlyoverHint of the next maneuver           */
    bool off_route;
    bool link_lost;          /* no packets from the phone at all             */
    bool gps_stale;          /* link fine, but no fresh position             */
    bool no_line;            /* Google navigates, ROTOS has no route line    */
    bool have_clock;         /* time_s valid (the phone sent the time)       */
    uint32_t time_s;         /* local seconds of day, now                    */
    const char *busy;        /* background work worth a small chip, or NULL  */
    RtStatusView status;
} RtHudData;

typedef struct {
    bool have_clock;
    uint32_t time_s;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    bool have_weather;
    RnWeather weather;
    RtStatusView status;
    const char *footer;      /* NULL = default for the link state            */
} RtDashData;

typedef struct {
    float distance_m;
    uint32_t ride_s;
    uint32_t moving_s;
    float avg_kmh;
    float max_kmh;
    bool have_clock;
    uint32_t start_s;
    uint32_t end_s;
} RtRideSummary;

typedef struct {
    bool have_clock;
    uint32_t time_s;
    bool have_weather;
    RnWeather weather;
    bool have_forecast;
    RnForecast forecast;
    uint32_t forecast_age_s;
    bool night;
    RtStatusView status;
} RtWeatherPage;

/* Google Maps mode: the next turn from GUIDANCE, no map. */
typedef struct {
    uint8_t maneuver;        /* RnManeuverType                               */
    bool arrived;
    uint16_t to_turn_m;      /* RN_GUIDE_UNKNOWN16 when not known            */
    uint32_t remaining_m;    /* RN_GUIDE_UNKNOWN32 when not known            */
    uint32_t eta_s;          /* local seconds of day, RN_GUIDE_UNKNOWN32     */
    const char *road;
    RtStatusView status;
} RtGuideView;

/* Idle screen (no phone): stored ride statistics and time since power-on. */
typedef struct {
    uint32_t odo_m;
    uint32_t last_m;
    uint32_t last_min;
    uint32_t rides;
    uint32_t uptime_s;
} RtIdleView;

typedef struct {
    uint8_t ui;              /* RtCallUi (rt_call.h)                         */
    const char *name;
} RtCallView;

/* Notifications page: one notification at a time, newest first. */
typedef struct {
    const RnNotify *item;    /* shown, or NULL when there are none            */
    uint32_t index;          /* 0 = newest                                    */
    uint32_t count;
    uint32_t age_s;          /* since it arrived                              */
    RtStatusView status;
} RtNotifyView;

/* Page indicator: n dots, the current one filled. */
void rt_draw_page_dots(RtCanvas *cv, uint32_t n, uint32_t current);
void rt_draw_weather(RtCanvas *cv, const RtTheme *t, const RtWeatherPage *w, uint32_t now_ms);
/* Call bar at the bottom over the speed panel; map and next turn stay visible. */
void rt_draw_call(RtCanvas *cv, const RtTheme *t, const RtCallView *c, uint32_t now_ms);
void rt_draw_boot(RtCanvas *cv, const RtTheme *t, uint32_t ms);
void rt_draw_guidance(RtCanvas *cv, const RtTheme *t, const RtGuideView *g, uint32_t now_ms);
void rt_draw_waiting(RtCanvas *cv, const RtTheme *t, const RtIdleView *d, uint32_t ms);
void rt_draw_dashboard(RtCanvas *cv, const RtTheme *t, const RtDashData *d, uint32_t now_ms);
void rt_draw_hud(RtCanvas *cv, const RtTheme *t, const RtHudData *d, uint32_t now_ms);
void rt_draw_summary(RtCanvas *cv, const RtTheme *t, const RtRideSummary *r, uint32_t seconds_left);
void rt_draw_notify_page(RtCanvas *cv, const RtTheme *t, const RtNotifyView *v, uint32_t now_ms);
/* A new notification, briefly, as a bar at the bottom (like the call bar). */
void rt_draw_notify_toast(RtCanvas *cv, const RtTheme *t, const RnNotify *n, uint32_t now_ms);
/* Overlay while a map pack is arriving. */
void rt_draw_map_progress(RtCanvas *cv, const RtTheme *t, uint32_t received, uint32_t total, uint32_t now_ms);
/* Same panel while the phone prepares the route (PREP_STATUS, RN_PREP_*). */
void rt_draw_prep_progress(RtCanvas *cv, const RtTheme *t, uint8_t stage, uint8_t pct, uint16_t done,
                           uint16_t total, uint32_t now_ms);

#endif /* RT_SCREENS_H */
