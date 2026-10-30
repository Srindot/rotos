/*
 * rt_app.c - initialisation, state machine and rendering.
 */
#include "rt_app.h"

#include "rn/rn_crc.h"

#include <math.h>
#include <string.h>

#include "hal_display.h"
#include "hal_input.h"
#include "hal_log.h"
#include "hal_storage.h"
#include "hal_time.h"
#include "rn/rn_types.h"
#include "rt_ili9341.h"
#include "rt_navview.h"
#include "rt_settings.h"

void rt_app_load_pack(RtApp *app)
{
    if (!RN_ASSERT(app != NULL)) {
        return;
    }
    uint32_t len = 0u;
    const uint8_t *blob = hal_storage_map(&len);
    if (app->have_pack && blob == app->pack.blob && len == app->pack.len) {
        return;                                       /* same mapping: no CRC pass, keep the tile cache */
    }
    app->have_pack = blob != NULL && rn_pack_open(&app->pack, blob, len) == RN_OK;
    app->pack_crc = app->have_pack ? rn_crc32(blob, len) : 0u;
    rn_tile_cache_init(app->cache);
    hal_log("app", app->have_pack ? "map pack loaded, bytes" : "no map pack", (int32_t)len);
}

void rt_app_apply_settings(RtApp *app, const RnSettings *s, bool save)
{
    if (!RN_ASSERT(app != NULL && s != NULL)) {
        return;
    }
    app->settings = *s;
    rt_themectl_set_mode(&app->theme, s->theme_mode);
    rt_present_set_glass(&app->presenter, (int32_t)s->glass_pct);
    if (save && !rt_settings_save(s)) {
        hal_log("app", "settings not saved", 0);
    }
}

/*
 * Power came back with a ride in flash (engine off at a signal): straight to
 * the riding map at the last position, the ride's distance and times carry
 * on. Whether it really is the same ride is decided once the phone's clock
 * says how long ROTOS was off (decide_resume).
 */
static void restore_ride(RtApp *app, uint32_t now_ms)
{
    if (!rt_resume_load(&app->resume)) {
        return;
    }
    const RtResumeSnap *s = &app->resume;
    app->device_flags = (uint8_t)RN_DEVICE_RIDE_FOUND;
    app->off_s = RN_DEVICE_OFF_UNKNOWN;
    rt_ride_restore(&app->ride, now_ms, s->distance_m, s->ride_ms, s->moving_ms, s->max_mps,
                    s->have_clock != 0u, s->start_s);
    if (app->have_pack && s->nav.route_id == app->pack.h.route_id) {
        rn_tracker_reset(&app->tracker);
        rn_tracker_on_nav(&app->tracker, &s->nav, now_ms);
    }
    rt_phone_on_night(&app->phone, (s->nav.flags & RN_NAV_NIGHT) != 0u);   /* the theme it rode in */
    app->state = RT_ST_NAVIGATING;
    app->page = RT_PAGE_MAP;
    app->page_ms = now_ms;
    app->nav_seen_ms = now_ms;
    app->restore_ms = now_ms;
    app->resume_pending = true;
    hal_log("app", "power back with a ride in progress, km x10", (int32_t)(s->distance_m / 100.0f));
}

void rt_app_init(RtApp *app, RnTileCache *cache, RnPackPoint *line_buf, uint32_t now_ms)
{
    if (!RN_ASSERT(app != NULL && cache != NULL)) {
        return;
    }
    (void)memset(app, 0, sizeof *app);
    app->cache = cache;
    rt_routerx_init(&app->line, line_buf);
    app->state = RT_ST_BOOT;
    app->boot_ms = now_ms;
    rt_present_init(&app->presenter);
    RnSettings s = rt_settings_load();
    rt_themectl_init(&app->theme, s.theme_mode);
    rt_app_apply_settings(app, &s, false);
    rt_link_init(&app->link);
    rt_phone_init(&app->phone);
    rt_maprx_init(&app->maprx);
    rn_tracker_reset(&app->tracker);
    rt_input_init(&app->input);
    rt_backlight_init(&app->backlight);
    rt_call_init(&app->call);
    rt_notify_init(&app->notify);
    app->page = RT_PAGE_DASHBOARD;
    hal_time_sleep_until(now_ms + RT_POWER_SETTLE_MS);   /* bike power still rising at ignition */
    rt_ili9341_init();
    app->panel_ms = hal_time_ms();
    app->stats = rt_stats_load();
    rt_app_load_pack(app);
    restore_ride(app, hal_time_ms());
}

static void start_ride(RtApp *app, uint32_t now_ms)
{
    uint32_t clock = 0u;
    bool have_clock = rt_phone_clock(&app->phone, now_ms, &clock);
    app->ride_requested = false;
    app->page = RT_PAGE_MAP;
    app->page_ms = now_ms;
    if (!app->ride.active) {                         /* a restored ride (power cut) carries on */
        rt_ride_start(&app->ride, now_ms, have_clock, clock);
    }
    app->state = RT_ST_NAVIGATING;
    app->nav_seen_ms = now_ms;
    hal_log("app", "ride started, route", (int32_t)app->pack.h.route_id);
}

/* [quiet]: a ride closed in the background (an old one found at boot): statistics, no card. */
static void finish_ride(RtApp *app, uint32_t now_ms, bool quiet)
{
    rt_resume_clear();                               /* over: nothing to carry across a power cut */
    app->resume_pending = false;
    uint32_t clock = 0u;
    bool have_clock = rt_phone_clock(&app->phone, now_ms, &clock);
    app->summary = rt_ride_finish(&app->ride, now_ms, have_clock, clock);
    app->summary_ms = now_ms;
    app->state = quiet ? RT_ST_DASHBOARD : RT_ST_SUMMARY;
    app->page = RT_PAGE_DASHBOARD;
    if (quiet) {
        rn_tracker_reset(&app->tracker);
    }
    hal_log("app", "arrived, distance m", (int32_t)app->summary.distance_m);
    if (!rt_stats_add_ride(&app->stats, app->summary.distance_m, app->summary.ride_s)) {
        hal_log("app", "ride statistics not saved", 0);
    }
}

/* Off time vs re-engage time: same ride if ROTOS was off briefly, else the old ride is closed. */
static void decide_resume(RtApp *app, uint32_t now_ms)
{
    uint32_t clock = 0u;
    if (!app->resume_pending || !app->phone.have_status || !rt_phone_clock(&app->phone, now_ms, &clock)) {
        return;
    }
    app->resume_pending = false;
    uint32_t off = rt_resume_off_s(&app->resume, clock, app->phone.status.day, app->phone.status.month);
    app->off_s = (off == RT_RESUME_OFF_UNKNOWN) ? RN_DEVICE_OFF_UNKNOWN : off;
    if (off != RT_RESUME_OFF_UNKNOWN && off <= RT_RESUME_MAX_S) {
        /* the ride time includes the stop: the time ROTOS was really off (the clock has been
         * running again since restore_ride) */
        uint32_t since = now_ms - app->restore_ms;
        uint32_t dark_ms = (off * 1000u > since) ? off * 1000u - since : 0u;
        app->ride.start_ms -= dark_ms;
        app->device_flags |= (uint8_t)RN_DEVICE_RIDE_RESUMED;
        hal_log("app", "same ride: ROTOS was off, s", (int32_t)off);
    } else {
        app->device_flags |= (uint8_t)RN_DEVICE_RIDE_CLOSED;
        hal_log("app", "off too long: the old ride is closed, s", (int32_t)off);
        finish_ride(app, now_ms, true);              /* its statistics, no card over today's ride */
    }
    RnDeviceStatus st = {now_ms, app->reset_reason, app->device_flags, app->off_s};
    (void)rt_link_send_device_status(&app->link, now_ms, &st);
}

/* Every RT_RESUME_SAVE_MS while riding (never before the resume is decided: its time is the evidence). */
static void save_snapshot(RtApp *app, uint32_t now_ms)
{
    uint32_t clock = 0u;
    /* only while positions are fresh: a ride that stopped (arrived, phone gone) must not look in progress */
    if (app->resume_pending || now_ms - app->snap_ms < RT_RESUME_SAVE_MS || !app->ride.active ||
        rn_tracker_age_ms(&app->tracker, now_ms) > RT_NAV_TIMEOUT_MS ||
        !app->phone.have_status || !rt_phone_clock(&app->phone, now_ms, &clock)) {
        return;
    }
    app->snap_ms = now_ms;
    RtResumeSnap s = rt_resume_snap(&app->ride, &app->tracker.last, now_ms, clock, app->phone.status.day,
                                    app->phone.status.month);
    if (!rt_resume_save(&s)) {
        hal_log("app", "ride snapshot not saved", 0);
    }
}

static void step_navigation(RtApp *app, uint32_t now_ms)
{
    if (!app->have_pack || !app->tracker.have) {
        return;                                      /* new map on its way */
    }
    save_snapshot(app, now_ms);
    rn_tracker_update(&app->tracker, &app->pack, now_ms);
    bool free = app->tracker.free;                   /* Google Maps navigates */
    rt_ride_update(&app->ride, free ? app->tracker.odo_m : app->tracker.along_m, app->tracker.speed_mps, now_ms);
    bool arrived_flag = (app->tracker.last.flags & RN_NAV_ARRIVED) != 0u;
    bool at_end = !free && app->pack.h.total_m - app->tracker.along_m < RT_ARRIVE_M &&
                  app->tracker.speed_mps < 1.0f;
    if (arrived_flag || at_end) {
        finish_ride(app, now_ms, false);
    }
}

/* Every link-up: ROTOS's uptime and why it started, so the phone can log a power cut vs a BLE drop. */
static void send_status_on_link_up(RtApp *app, uint32_t now_ms)
{
    bool ok = rt_link_ok(&app->link, now_ms);
    if (ok && !app->link_was_ok) {
        app->link_up_ms = now_ms;
        app->linked_since_boot = true;
        RnDeviceStatus st = {now_ms, app->reset_reason, app->device_flags,
                             app->resume_pending ? RN_DEVICE_OFF_UNKNOWN : app->off_s};
        (void)rt_link_send_device_status(&app->link, now_ms, &st);
    }
    app->link_was_ok = ok;
}

/*
 * Riding: the phone ends a ride itself (ARRIVED). No positions only ends it
 * after RT_RESUME_MAX_S (tunnel, lost GPS, Bluetooth or power gone), counted
 * from the last position, link-up or map chunk.
 */
static void step_ride(RtApp *app, uint32_t now_ms)
{
    if (app->state != RT_ST_NAVIGATING) {
        return;
    }
    if (app->maprx.active) {
        app->nav_seen_ms = now_ms;                   /* a new map on its way is not a stopped ride */
    }
    step_navigation(app, now_ms);
    uint32_t seen = app->nav_seen_ms;
    if (app->link_up_ms != 0u && (int32_t)(app->link_up_ms - seen) > 0) {
        seen = app->link_up_ms;
    }
    if (app->state == RT_ST_NAVIGATING && (int32_t)(now_ms - seen) > (int32_t)(RT_RESUME_MAX_S * 1000u)) {
        hal_log("app", "no positions for 10 min: ride over", 0);
        finish_ride(app, now_ms, app->resume_pending);   /* never confirmed: no card */
        return;
    }
    /* restored at boot but no phone for a minute (maybe it was left at home): the dashboard,
     * the ride kept pending in case it turns up */
    if (app->resume_pending && !app->linked_since_boot && now_ms - app->restore_ms > 60000u) {
        app->state = RT_ST_DASHBOARD;
    }
}

void rt_app_step(RtApp *app, uint32_t now_ms)
{
    if (!RN_ASSERT(app != NULL)) {
        return;
    }
    rt_themectl_update(&app->theme, app->phone.have_night, app->phone.night, now_ms);
    /* ready the free map slot, a sector at a time: every frame when idle, every 4th while
     * riding (each sector erase stalls a frame ~40 ms) */
    if (!app->maprx.active && (app->state != RT_ST_NAVIGATING || (app->frames & 3u) == 0u)) {
        hal_storage_service();
    }
    if (app->state == RT_ST_BOOT && now_ms - app->boot_ms >= RT_BOOT_MS) {
        app->state = RT_ST_WAITING;
    }
    if (app->state == RT_ST_WAITING && (app->phone.have_status || app->ride_requested)) {
        app->state = RT_ST_DASHBOARD;
    }
    if (app->state == RT_ST_DASHBOARD && app->ride_requested && app->have_pack) {
        start_ride(app, now_ms);
    }
    decide_resume(app, now_ms);                      /* a ride restored at boot: same ride or closed */
    step_ride(app, now_ms);
    send_status_on_link_up(app, now_ms);
    /* signed: last_rx_ms may be later than this frame's now_ms (stamped after a long flash erase) */
    if (app->maprx.active && (int32_t)(now_ms - app->maprx.last_rx_ms) > (int32_t)RT_MAPRX_STALL_MS) {
        hal_log("maprx", "transfer abandoned, bytes", (int32_t)app->maprx.received);
        app->maprx.active = false;
        app->maprx.failed++;
        hal_storage_map_abort();                     /* the current map stays */
    }
    if (app->state == RT_ST_SUMMARY && now_ms - app->summary_ms >= RT_SUMMARY_MS) {
        app->state = RT_ST_DASHBOARD;
    }
    rt_call_update(&app->call, now_ms);
    bool moving = app->state == RT_ST_NAVIGATING && app->tracker.speed_mps > RT_PAGE_RETURN_MPS;
    if (moving && app->page != RT_PAGE_MAP && now_ms - app->page_ms > RT_PAGE_RETURN_MS) {
        app->page = RT_PAGE_MAP;                     /* eyes back on the road */
        app->page_ms = now_ms;
    }
    uint8_t pct = rt_backlight_update(&app->backlight, hal_input_light(), app->settings.brightness_pct,
                                      app->theme.night, now_ms);
    if (pct != app->backlight_pct) {
        app->backlight_pct = pct;
        hal_display_backlight(pct);
    }
    rt_link_keepalive(&app->link, now_ms);
}

static void render_dashboard(RtApp *app, const RtTheme *t, uint32_t now_ms)
{
    RtDashData d;
    (void)memset(&d, 0, sizeof d);
    d.have_clock = rt_phone_clock(&app->phone, now_ms, &d.time_s);
    d.weekday = app->phone.status.weekday;
    d.day = app->phone.status.day;
    d.month = app->phone.status.month;
    d.have_weather = app->phone.have_weather;
    d.weather = app->phone.weather;
    d.weather.is_day = app->phone.weather.is_day && !app->phone.night;   /* moon after sunset */
    d.status = rt_phone_view(&app->phone, rt_link_ok(&app->link, now_ms));
    d.footer = NULL;
    if (app->state == RT_ST_NAVIGATING) {
        d.footer = app->have_pack ? "RIDING - PRESS A FOR THE MAP" : "WAITING FOR MAP";
    } else if (app->wrong_map && d.status.link_ok) {
        d.footer = "WAITING FOR MAP";
    } else {
        /* default footer for the link state */
    }
    rt_draw_dashboard(&app->canvas, t, &d, now_ms);
}

static void render_guidance(RtApp *app, const RtTheme *t, uint32_t now_ms)
{
    const RnGuidance *g = &app->phone.guide;
    RtGuideView v;
    v.maneuver = g->maneuver;
    v.arrived = (g->flags & RN_GUIDE_ARRIVED) != 0u;
    v.to_turn_m = g->to_turn_m;
    v.remaining_m = g->remaining_m;
    v.eta_s = g->eta_s;
    v.road = g->road;
    v.status = rt_phone_view(&app->phone, rt_link_ok(&app->link, now_ms));
    rt_draw_guidance(&app->canvas, t, &v, now_ms);
}

static void render_weather(RtApp *app, const RtTheme *t, uint32_t now_ms)
{
    static RtWeatherPage w;                          /* ~70 B forecast: off the stack */
    (void)memset(&w, 0, sizeof w);
    w.have_clock = rt_phone_clock(&app->phone, now_ms, &w.time_s);
    w.have_weather = app->phone.have_weather;
    w.weather = app->phone.weather;
    w.have_forecast = app->phone.have_forecast;
    w.forecast = app->phone.forecast;
    w.forecast_age_s = (now_ms - app->phone.forecast_rx_ms) / 1000u;
    w.night = app->phone.night;
    w.status = rt_phone_view(&app->phone, rt_link_ok(&app->link, now_ms));
    rt_draw_weather(&app->canvas, t, &w, now_ms);
}

static void render_notify(RtApp *app, const RtTheme *t, uint32_t now_ms)
{
    RtNotifyView v;
    uint32_t rx = now_ms;
    v.item = rt_notify_get(&app->notify, app->notify.cursor, &rx);
    v.index = app->notify.cursor;
    v.count = app->notify.count;
    v.age_s = (now_ms - rx) / 1000u;
    v.status = rt_phone_view(&app->phone, rt_link_ok(&app->link, now_ms));
    rt_draw_notify_page(&app->canvas, t, &v, now_ms);
}

static void render_page_dots(RtApp *app)
{
    bool riding = app->state == RT_ST_NAVIGATING;
    uint32_t n = riding ? 4u : 3u;
    uint32_t current = riding ? (uint32_t)app->page : (uint32_t)app->page - 1u;
    if (app->page != RT_PAGE_MAP && app->state != RT_ST_SUMMARY) {
        rt_draw_page_dots(&app->canvas, n, current);
    }
}

static void render_navigation(RtApp *app, const RtTheme *t, uint32_t now_ms, bool overlays)
{
    RtNavViewOpts o;
    o.north_up = app->settings.north_up != 0u;
    o.overlays = overlays;
    o.link_lost = !rt_link_ok(&app->link, now_ms);           /* the phone went quiet */
    o.gps_stale = !o.link_lost && rn_tracker_age_ms(&app->tracker, now_ms) > RT_NAV_TIMEOUT_MS;
    o.status = rt_phone_view(&app->phone, rt_link_ok(&app->link, now_ms));
    o.guide = rt_phone_guidance_active(&app->phone, now_ms) ? &app->phone.guide : NULL;
    o.guide_age_ms = now_ms - app->phone.guide_dist_ms;
    RnLine line;
    o.line = rt_routerx_line(&app->line, app->pack.h.route_id, &line) ? &line : NULL;
    o.time_s = 0u;
    o.have_clock = rt_phone_clock(&app->phone, now_ms, &o.time_s);
    o.busy = NULL;
    uint8_t stage = rt_phone_prep_active(&app->phone, now_ms) ? app->phone.prep.stage : (uint8_t)RN_PREP_IDLE;
    if ((app->tracker.last.flags & RN_NAV_REROUTING) != 0u) {
        o.busy = "REROUTING";
    } else if (app->maprx.active || (stage >= (uint8_t)RN_PREP_ROUTE && stage <= (uint8_t)RN_PREP_MAP)) {
        o.busy = "UPDATING MAP";                     /* the map in flash stays on screen meanwhile */
    } else {
        /* nothing in the background */
    }
    rt_navview_draw(&app->canvas, t, &app->pack, app->cache, &app->tracker, &o, now_ms);
}

/* Call bar / notification toast, then progress (a panel, except over the riding map). */
static void render_popups(RtApp *app, const RtTheme *t, uint32_t now_ms, bool riding_map)
{
    RtCanvas *cv = &app->canvas;
    if (rt_call_visible(&app->call) && app->state >= RT_ST_DASHBOARD) {
        RtCallView cvw = {app->call.ui, app->call.call.name};
        rt_draw_call(cv, t, &cvw, now_ms);
    } else if (app->state >= RT_ST_DASHBOARD && app->page != RT_PAGE_NOTIFY) {
        rt_draw_notify_toast(cv, t, rt_notify_toast(&app->notify, now_ms), now_ms);
    } else {
        /* no popup */
    }
    if (riding_map) {
        /* riding: background work is a small chip in the HUD, never a panel over speed / ETA */
    } else if (app->maprx.active) {
        rt_draw_map_progress(cv, t, app->maprx.received, app->maprx.total, now_ms);
    } else if (rt_phone_prep_active(&app->phone, now_ms)) {
        const RnPrepStatus *ps = &app->phone.prep;
        rt_draw_prep_progress(cv, t, ps->stage, ps->pct, ps->done, ps->total, now_ms);
    } else {
        /* nothing in progress */
    }
}

void rt_app_render(RtApp *app, uint32_t now_ms)
{
    if (!RN_ASSERT(app != NULL)) {
        return;
    }
    const RtTheme *t = rt_themectl_theme(&app->theme);
    RtCanvas *cv = &app->canvas;
    bool map_ok = app->have_pack && app->tracker.have;
    bool riding_map = app->state == RT_ST_NAVIGATING && map_ok && app->page == RT_PAGE_MAP;
    rt_canvas_begin(cv);
    if (app->state == RT_ST_BOOT) {
        rt_draw_boot(cv, t, now_ms - app->boot_ms);
    } else if (app->state == RT_ST_WAITING) {
        RtIdleView idle = {app->stats.odo_m, app->stats.last_m, app->stats.last_min, app->stats.rides,
                           (now_ms - app->boot_ms) / 1000u};
        rt_draw_waiting(cv, t, &idle, now_ms);
    } else if (riding_map) {
        render_navigation(app, t, now_ms, true);
    } else if (app->page == RT_PAGE_WEATHER && app->state != RT_ST_SUMMARY) {
        render_weather(app, t, now_ms);
    } else if (app->page == RT_PAGE_NOTIFY && app->state != RT_ST_SUMMARY) {
        render_notify(app, t, now_ms);
        render_page_dots(app);
    } else if (app->state == RT_ST_DASHBOARD && rt_phone_guidance_active(&app->phone, now_ms)) {
        render_guidance(app, t, now_ms);              /* Google Maps navigating */
        render_page_dots(app);
    } else if (app->state == RT_ST_SUMMARY && map_ok) {
        render_navigation(app, t, now_ms, false);     /* map frozen behind the card */
        uint32_t shown = now_ms - app->summary_ms;
        uint32_t left = (shown < RT_SUMMARY_MS) ? (RT_SUMMARY_MS - shown + 999u) / 1000u : 0u;
        rt_draw_summary(cv, t, &app->summary, left);
    } else {
        render_dashboard(app, t, now_ms);
        render_page_dots(app);
    }
    render_popups(app, t, now_ms, riding_map);
    /* A cranking dip can reset the panel while the ESP32 runs on: re-send its
     * setup now and then and repaint everything, so a white screen heals itself. */
    bool refresh = now_ms - app->panel_ms >= RT_PANEL_REFRESH_MS;
    if (refresh) {
        rt_ili9341_refresh();
        app->panel_ms = now_ms;
    }
    rt_present_frame(&app->presenter, cv, t, refresh);
    app->frames++;
}
