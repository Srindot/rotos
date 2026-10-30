/*
 * rt_app.h - the ROTOS application: one static instance, driven by main().
 *
 *   Boot --> Waiting --(phone heard)--> Dashboard --(ride starts)--> Navigating
 *                                          ^                              | arrived
 *                                          +------ 15 s ---- Summary <----+
 *
 * main() calls, every frame and in this order:
 *   rt_app_service_link   bytes from the phone -> packets -> model
 *   rt_app_service_input  buttons -> actions
 *   rt_app_step           state machine, theme, ride statistics, backlight
 *   rt_app_render         draw the current screen and send changed bands
 *
 * Platform-free: everything outside goes through hal_*.h.
 */
#ifndef RT_APP_H
#define RT_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_mappack.h"
#include "rn/rn_protocol.h"
#include "rn/rn_tracker.h"
#include "rt_backlight.h"
#include "rt_call.h"
#include "rt_canvas.h"
#include "rt_input.h"
#include "rt_link.h"
#include "rt_maprx.h"
#include "rt_notify.h"
#include "rt_phone.h"
#include "rt_present.h"
#include "rt_resume.h"
#include "rt_ride.h"
#include "rt_routerx.h"
#include "rt_screens.h"
#include "rt_settings.h"
#include "rt_themectl.h"

typedef enum {
    RT_ST_BOOT = 0,
    RT_ST_WAITING,
    RT_ST_DASHBOARD,
    RT_ST_NAVIGATING,
    RT_ST_SUMMARY
} RtState;

/* Pages cycled with button A. Riding: map -> dashboard -> weather ->
 * notifications; idle: dashboard -> weather -> notifications. */
typedef enum {
    RT_PAGE_MAP = 0,
    RT_PAGE_DASHBOARD,
    RT_PAGE_WEATHER,
    RT_PAGE_NOTIFY
} RtPage;

typedef struct {
    RtState state;
    uint32_t boot_ms;
    uint32_t summary_ms;
    uint8_t page;             /* RtPage                                 */
    uint32_t page_ms;         /* when the page was last changed         */
    RtCall call;              /* incoming call popup                    */
    RtNotify notify;          /* phone notifications (page + toast)     */
    RnSettings settings;
    RtCanvas canvas;
    RtPresenter presenter;
    RtThemeCtl theme;
    RtLink link;
    RtPhone phone;
    RtMapRx maprx;
    RnPack pack;
    bool have_pack;
    uint32_t pack_crc;        /* CRC-32 of the stored pack (skip identical uploads) */
    RnTileCache *cache;       /* 64 KB, owned by main (static or boot heap) */
    RtRouteRx line;           /* route line received at run time (reroutes) */
    RnTracker tracker;
    bool ride_requested;
    uint32_t nav_seen_ms;     /* last NAV_STATE for our pack, ride start or new map */
    bool wrong_map;           /* NAV_STATE for a route we do not have    */
    RtRide ride;
    RtRideSummary summary;
    RtInput input;
    RtBacklight backlight;
    uint8_t backlight_pct;
    RtStats stats;            /* odometer + last ride, kept in flash     */
    uint32_t frames;
    uint32_t panel_ms;        /* last panel refresh (rt_ili9341_refresh) */
    /* power cuts (engine off at a signal): the ride goes on, see rt_resume */
    RtResumeSnap resume;      /* the snapshot found at boot               */
    bool resume_pending;      /* restored at boot; off time not known yet */
    uint32_t snap_ms;         /* last snapshot save                       */
    uint8_t reset_reason;     /* why ROTOS (re)started (esp_reset_reason) */
    uint8_t device_flags;     /* RN_DEVICE_RIDE_*                          */
    uint32_t off_s;           /* how long ROTOS was off, once known       */
    bool link_was_ok;         /* for DEVICE_STATUS on every link-up       */
    uint32_t link_up_ms;      /* last link-up: a reconnect is not a stopped ride */
    uint32_t restore_ms;      /* when restore_ride ran (boot)             */
    bool linked_since_boot;   /* the phone was there at least once        */
} RtApp;

/* cache: decoded-tile cache storage. It is kept outside RtApp so a target
 * with a small static-RAM limit (classic ESP32) can take it from the heap
 * once at boot. Must stay valid for the program's lifetime. */
/* line_buf: RN_ROUTE_MAX_PTS points for run-time route lines (12 KB), also owned by main. */
void rt_app_init(RtApp *app, RnTileCache *cache, RnPackPoint *line_buf, uint32_t now_ms);
void rt_app_service_link(RtApp *app, uint32_t now_ms);
void rt_app_service_input(RtApp *app, uint32_t now_ms);
void rt_app_step(RtApp *app, uint32_t now_ms);
void rt_app_render(RtApp *app, uint32_t now_ms);

/* Internal, shared between rt_app*.c files. */
void rt_app_on_packet(RtApp *app, const RnPacket *p, uint32_t now_ms);
void rt_app_load_pack(RtApp *app);
void rt_app_apply_settings(RtApp *app, const RnSettings *s, bool save);

#endif /* RT_APP_H */
