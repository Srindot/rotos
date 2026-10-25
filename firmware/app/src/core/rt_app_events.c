/*
 * rt_app_events.c - what ROTOS does with packets from the phone and with
 * button presses.
 */
#include <string.h>

#include "hal_input.h"
#include "hal_log.h"
#include "rn/rn_types.h"
#include "rt_app.h"

#include "hal_storage.h"

static void on_nav(RtApp *app, const RnNavState *s, uint32_t now_ms)
{
    rt_phone_on_night(&app->phone, (s->flags & RN_NAV_NIGHT) != 0u);
    bool ours = app->have_pack && s->route_id == app->pack.h.route_id;
    app->wrong_map = !ours;
    if (!ours) {
        return;                                   /* map for this route not here yet */
    }
    app->nav_seen_ms = now_ms;
    if (app->state == RT_ST_NAVIGATING) {
        rn_tracker_on_nav(&app->tracker, s, now_ms);
        return;
    }
    bool free = (s->flags & RN_NAV_FREE) != 0u;      /* Google Maps navigates */
    bool ride_left = (free || s->along_m < app->pack.h.total_m - 20.0f) && (s->flags & RN_NAV_ARRIVED) == 0u;
    if (app->state != RT_ST_SUMMARY && ride_left) {
        rn_tracker_reset(&app->tracker);          /* a ride begins (or resumes after a restart) */
        rn_tracker_on_nav(&app->tracker, s, now_ms);
        app->ride_requested = true;
    }
}

static void on_map_reply(RtApp *app, const RnPacket *p, const RtMapRxReply *r, uint32_t now_ms)
{
    if (r->send_ack) {
        (void)rt_link_send_ack(&app->link, now_ms, p->type, p->seq, r->status, r->value);
    }
    if (r->committed) {
        rt_app_load_pack(app);
        rn_tracker_reset(&app->tracker);          /* positions refer to the new route */
        app->nav_seen_ms = now_ms;                /* a new map is not an abandoned ride */
        rt_ride_rebase(&app->ride);
    }
}

static void on_map(RtApp *app, const RnPacket *p, uint32_t now_ms)
{
    RtMapRxReply r = {false, RN_ACK_OK, 0u, false};
    if (p->type == RN_PKT_MAP_BEGIN) {
        RnMapBegin b;
        if (rn_map_begin_decode(p, &b) != RN_OK) {
            return;
        }
        bool same = app->have_pack && b.total_len == app->pack.len && b.crc32 == app->pack_crc;
        if (same) {                               /* already stored: no upload needed */
            hal_log("maprx", "map already stored, bytes", (int32_t)b.total_len);
            (void)rt_link_send_ack(&app->link, now_ms, p->type, p->seq, RN_ACK_HAVE, b.total_len);
            return;
        }
        uint32_t blen = 0u;
        const uint8_t *base = app->have_pack ? hal_storage_map(&blen) : NULL;
        r = rt_maprx_begin(&app->maprx, &b, now_ms, base, blen, app->pack_crc);
        if (r.status == RN_ACK_NO_BASE) {
            hal_log("maprx", "patch for a pack not held: asking for the whole map", 0);
        }
        rt_app_load_pack(app);                    /* the current map stays (unless the new one needs all slots) */
    } else if (p->type == RN_PKT_MAP_CHUNK) {
        static RnMapChunk c;                      /* 246 B: keep it off the stack */
        if (rn_map_chunk_decode(p, &c) != RN_OK) {
            return;
        }
        r = rt_maprx_chunk(&app->maprx, &c);
        app->maprx.last_rx_ms = now_ms;            /* still arriving: no stall timeout */
    } else {
        RnMapEnd e;
        if (rn_map_end_decode(p, &e) != RN_OK) {
            return;
        }
        r = rt_maprx_end(&app->maprx, &e);
        hal_log("maprx", (r.status == RN_ACK_OK) ? "map stored" : "map rejected", (int32_t)r.value);
    }
    on_map_reply(app, p, &r, now_ms);
}

void rt_app_on_packet(RtApp *app, const RnPacket *p, uint32_t now_ms)
{
    if (!RN_ASSERT(app != NULL && p != NULL)) {
        return;
    }
    RnNavState nav;
    RnPhoneStatus st;
    RnWeather w;
    RnSettings s;
    static RnForecast f;
    RnCallState call;
    RnPrepStatus prep;
    RnGuidance guide;
    static RnNotify note;                         /* 170 B: keep it off the stack */
    static RnRouteChunk chunk;                    /* 233 B */
    if (p->type == RN_PKT_ROUTE && rn_route_chunk_decode(p, &chunk) == RN_OK) {
        if (rt_routerx_on(&app->line, &chunk)) {  /* a new line is complete: confirm it */
            (void)rt_link_send_ack(&app->link, now_ms, p->type, p->seq, RN_ACK_OK, chunk.route_id);
            hal_log("route", "new line, points", (int32_t)chunk.total);
        }
    } else if (p->type == RN_PKT_NOTIFY && rn_notify_decode(p, &note) == RN_OK) {
        rt_notify_on(&app->notify, &note, now_ms);
    } else if (p->type == RN_PKT_GUIDANCE && rn_guidance_decode(p, &guide) == RN_OK) {
        rt_phone_on_guidance(&app->phone, &guide, now_ms);
    } else if (p->type == RN_PKT_PREP_STATUS && rn_prep_status_decode(p, &prep) == RN_OK) {
        rt_phone_on_prep(&app->phone, &prep, now_ms);
    } else if (p->type == RN_PKT_FORECAST && rn_forecast_decode(p, &f) == RN_OK) {
        rt_phone_on_forecast(&app->phone, &f, now_ms);
    } else if (p->type == RN_PKT_CALL_STATE && rn_call_state_decode(p, &call) == RN_OK) {
        rt_call_on_state(&app->call, &call, now_ms);
    } else if (p->type == RN_PKT_NAV_STATE && rn_nav_state_decode(p, &nav) == RN_OK) {
        on_nav(app, &nav, now_ms);
    } else if (p->type == RN_PKT_PHONE_STATUS && rn_phone_status_decode(p, &st) == RN_OK) {
        rt_phone_on_status(&app->phone, &st, now_ms);
    } else if (p->type == RN_PKT_WEATHER && rn_weather_decode(p, &w) == RN_OK) {
        rt_phone_on_weather(&app->phone, &w);
    } else if (p->type == RN_PKT_SETTINGS && rn_settings_decode(p, &s) == RN_OK) {
        rt_app_apply_settings(app, &s, true);
    } else if (p->type == RN_PKT_MAP_BEGIN || p->type == RN_PKT_MAP_CHUNK || p->type == RN_PKT_MAP_END) {
        on_map(app, p, now_ms);
    } else {
        /* PING and unknown types: the timestamp update in rt_link is enough */
    }
}

void rt_app_service_link(RtApp *app, uint32_t now_ms)
{
    if (!RN_ASSERT(app != NULL)) {
        return;
    }
    static RnPacket pkt;                          /* 256 B: keep it off the stack */
    rt_link_begin_poll(&app->link);
    for (uint32_t i = 0u; i < RT_LINK_MAX_PER_POLL; i++) {
        if (!rt_link_next(&app->link, now_ms, &pkt)) {
            break;
        }
        rt_app_on_packet(app, &pkt, now_ms);
    }
}

/* A short: next page. Riding: map -> dashboard -> weather -> notifications;
 * idle: dashboard -> weather -> notifications. On the summary card: back to
 * the dashboard. */
static void next_page(RtApp *app, uint32_t now_ms)
{
    if (app->state == RT_ST_SUMMARY) {
        app->state = RT_ST_DASHBOARD;
        app->page = RT_PAGE_DASHBOARD;
    } else if (app->state == RT_ST_NAVIGATING) {
        app->page = (uint8_t)((app->page + 1u) % 4u);
    } else {
        app->page = (app->page >= RT_PAGE_NOTIFY) ? (uint8_t)RT_PAGE_DASHBOARD : (uint8_t)(app->page + 1u);
    }
    if (app->page == RT_PAGE_NOTIFY) {
        rt_notify_open(&app->notify);
    }
    app->page_ms = now_ms;
}

/* Notifications: C on a toast opens it; on the page B = older, C = newer,
 * hold B = clear. Returns true when the press was used. */
static bool notify_button(RtApp *app, const RtButtonEvent *ev, uint32_t now_ms)
{
    bool on_page = app->page == RT_PAGE_NOTIFY && app->state != RT_ST_SUMMARY;
    if (!on_page && ev->button == 2u && ev->event == RN_BUTTON_SHORT &&
        rt_notify_toast(&app->notify, now_ms) != NULL) {
        app->page = RT_PAGE_NOTIFY;
        app->page_ms = now_ms;
        rt_notify_open(&app->notify);
        return true;
    }
    if (!on_page || ev->button == 0u) {
        return false;
    }
    if (ev->button == 1u) {
        if (ev->event == RN_BUTTON_SHORT) {
            rt_notify_older(&app->notify);
        } else {
            rt_notify_clear(&app->notify);
        }
    } else if (ev->event == RN_BUTTON_SHORT) {
        rt_notify_newer(&app->notify);
    } else {
        /* hold C: nothing */
    }
    app->page_ms = now_ms;                            /* reading: stay on the page */
    return true;
}

/* While a call rings, A and B belong to the popup (Game Boy style A/B). */
static bool call_button(RtApp *app, const RtButtonEvent *ev, uint32_t now_ms)
{
    if (app->state < RT_ST_DASHBOARD || !rt_call_owns_buttons(&app->call) || ev->event != RN_BUTTON_SHORT ||
        ev->button > 1u) {                        /* popup only drawn from the dashboard on */
        return false;
    }
    uint16_t id = app->call.call.call_id;
    uint8_t action = rt_call_press(&app->call, ev->button, now_ms);
    if (action != 0u) {
        (void)rt_link_send_call_action(&app->link, now_ms, id, action);
        hal_log("call", (action == RN_ACTION_REJECT_REPLY) ? "reject + reply, call" : "dismissed, call", id);
    }
    return true;
}

static void on_button(RtApp *app, const RtButtonEvent *ev, uint32_t now_ms)
{
    RnSettings s = app->settings;
    if (call_button(app, ev, now_ms) || notify_button(app, ev, now_ms)) {
        return;
    }
    if (ev->button == 0u && ev->event == RN_BUTTON_SHORT) {
        next_page(app, now_ms);
        return;
    }
    if (ev->button == 0u) {
        s.north_up = (uint8_t)((s.north_up != 0u) ? 0u : 1u);
    } else if (ev->button == 1u && ev->event == RN_BUTTON_SHORT) {
        s.theme_mode = (uint8_t)((s.theme_mode + 1u) % 3u);
    } else if (ev->button == 1u) {
        s.glass_pct = (s.glass_pct >= 90u) ? 50u : (uint8_t)(s.glass_pct + 20u);
    } else {
        return;                                    /* button C: phone decides */
    }
    rt_app_apply_settings(app, &s, true);
}

void rt_app_service_input(RtApp *app, uint32_t now_ms)
{
    if (!RN_ASSERT(app != NULL)) {
        return;
    }
    RtButtonEvent ev[HAL_BUTTON_COUNT * 2u];
    uint32_t n = rt_input_poll(&app->input, hal_input_buttons(), now_ms, ev, HAL_BUTTON_COUNT * 2u);
    for (uint32_t i = 0u; i < n; i++) {
        on_button(app, &ev[i], now_ms);
        (void)rt_link_send_button(&app->link, now_ms, ev[i].button, ev[i].event);
        hal_log("input", (ev[i].event == RN_BUTTON_SHORT) ? "short press" : "long press", ev[i].button);
    }
}
