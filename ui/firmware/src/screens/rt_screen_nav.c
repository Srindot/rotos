/*
 * rt_screen_nav.c - navigation HUD: floating glass panels over the map.
 */
#include "rn/rn_maneuver.h"
#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_icons.h"
#include "rt_screens.h"

static const char *const FLY_TEXT[4] = {"", "TAKE FLYOVER", "STAY BELOW", "EXIT FLYOVER"};

/* Top: next maneuver icon, distance, road. */
static void draw_next(RtCanvas *cv, const RtTheme *t, const RtHudData *d, uint32_t now_ms)
{
    rt_panel(cv, 6, 6, 228, 64, t, true);
    rt_canvas_fill_rect(cv, 12, 12, 52, 52, RT_C_ACCENT);
    rt_status(cv, 228, 11, &d->status, false, now_ms);
    if (d->next == (uint8_t)RN_M_NONE) {             /* no turn: a short message, if any */
        if (d->next_road != NULL && d->next_road[0] != '\0') {
            (void)rt_text(cv, 74, 24, d->next_road, 1, RT_C_TEXT, false);
            (void)rt_text(cv, 74, 40, "TURNS APPEAR HERE", 1, RT_C_TEXT_DIM, false);
        }
        return;
    }
    char v[RT_VALUE_LEN];
    char u[RT_UNIT_LEN];
    char buf[64];
    rt_icon_maneuver(cv, 14, 14, d->next, 3, RT_C_PANEL);
    rt_fmt_distance(d->next_dist_m, v, u);
    int32_t x = rt_text(cv, 74, 14, v, 4, RT_C_TEXT, false);
    (void)rt_text(cv, x + 6, 28, u, 2, RT_C_TEXT_DIM, false);
    const char *road = (d->next_road != NULL && d->next_road[0] != '\0') ? d->next_road
                       : ((d->next == (uint8_t)RN_M_ARRIVE) ? "DESTINATION" : "");
    (void)rt_text(cv, 74, 52, rt_fit(road, 1, 154, buf, sizeof buf), 1, RT_C_ACCENT, false);
}

/* Under it: THEN box and the warning / flyover chip. */
static void draw_secondary(RtCanvas *cv, const RtTheme *t, const RtHudData *d, uint32_t now_ms)
{
    if (d->then != (uint8_t)RN_M_NONE) {
        rt_panel(cv, 6, 74, 62, 24, t, false);
        (void)rt_text(cv, 12, 83, "THEN", 1, RT_C_TEXT_DIM, false);
        rt_icon_maneuver(cv, 44, 78, d->then, 1, RT_C_TEXT);
    }
    bool blink = ((now_ms / 500u) & 1u) != 0u;
    if (d->link_lost) {
        rt_chip(cv, t, 172, 78, blink ? "NO PHONE LINK" : "", RT_C_HILITE);
    } else if (d->gps_stale) {
        rt_chip(cv, t, 172, 78, blink ? "WAITING FOR GPS" : "", RT_C_HILITE);
    } else if (d->busy != NULL) {           /* rerouting / a new map on its way: map stays */
        rt_chip(cv, t, 172, 78, d->busy, RT_C_ACCENT);
    } else if (d->off_route) {
        rt_chip(cv, t, 172, 78, "OFF ROUTE", RT_C_HILITE);
    } else if (d->no_line) {                 /* the blue line needs the destination */
        rt_chip(cv, t, 172, 78, "SHARE PLACE FOR LINE", RT_C_TEXT_DIM);
    } else if (d->flyover != (uint8_t)RN_FLY_NONE && d->flyover <= (uint8_t)RN_FLY_EXIT &&
               d->next_dist_m < RT_FLYOVER_CHIP_M) {
        const char *txt = FLY_TEXT[d->flyover];
        int32_t w = rt_text_width(txt, 1) + 24;
        int32_t x = 172 - w / 2;
        uint8_t col = (d->flyover == (uint8_t)RN_FLY_EXIT) ? RT_C_TEXT : RT_C_HILITE;
        rt_panel(cv, x, 76, w, 19, t, false);
        rt_icon_flyover_arrow(cv, x + 6, 82, d->flyover == (uint8_t)RN_FLY_TAKE, col);
        (void)rt_text(cv, x + 17, 82, txt, 1, col, false);
    } else {
        /* nothing to warn about */
    }
}

/* One bottom-bar column: value (scale 2) over a small label. */
static void column(RtCanvas *cv, int32_t cx, const char *value, const char *label)
{
    (void)rt_text_centered(cv, cx, 272, value, 2, RT_C_TEXT, false);
    (void)rt_text_centered(cv, cx, 297, label, 1, RT_C_TEXT_DIM, false);
}

/* Time to the destination: "25" MIN LEFT, or "1:05" H LEFT; "--" without a clock. */
static void time_left(const RtHudData *d, char *v, uint32_t cap, const char **label)
{
    *label = "MIN LEFT";
    v[0] = '\0';
    if (!d->have_clock || d->eta_s == 0u) {
        (void)rt_fmt_cat(v, cap, "--");
        return;
    }
    uint32_t left = (d->eta_s % 86400u + 86400u - d->time_s % 86400u) % 86400u;
    if (left > 12u * 3600u) {                         /* the ETA just passed (minute resolution) */
        (void)rt_fmt_cat(v, cap, "<1");
        return;
    }
    uint32_t min = (left + 59u) / 60u;
    if (min < 60u) {
        (void)rt_fmt_u32(v, cap, min, 1u);
        return;
    }
    char mm[4];
    *label = "H LEFT";
    (void)rt_fmt_u32(v, cap, min / 60u, 1u);
    (void)rt_fmt_cat(v, cap, ":");
    (void)rt_fmt_u32(mm, sizeof mm, min % 60u, 2u);
    (void)rt_fmt_cat(v, cap, mm);
}

/* Bottom: speed | time left | ETA | distance left. */
static void draw_bottom(RtCanvas *cv, const RtTheme *t, const RtHudData *d)
{
    char v[RT_VALUE_LEN];
    char u[RT_UNIT_LEN];
    char buf[24];
    const char *label = "";
    rt_panel(cv, 6, 260, 228, 54, t, true);
    for (int32_t y = 268; y < 306; y += 2) {                 /* dotted separators */
        rt_canvas_pixel(cv, 66, y, RT_C_TEXT_DIM);
        rt_canvas_pixel(cv, 122, y, RT_C_TEXT_DIM);
        rt_canvas_pixel(cv, 178, y, RT_C_TEXT_DIM);
    }
    v[0] = '\0';
    (void)rt_fmt_u32(v, sizeof v, (d->speed_kmh > 0) ? (uint32_t)d->speed_kmh : 0u, 1u);
    (void)rt_text_centered(cv, 36, 268, v, 3, RT_C_TEXT, false);
    (void)rt_text_centered(cv, 36, 297, "KM/H", 1, RT_C_TEXT_DIM, false);
    time_left(d, v, sizeof v, &label);
    column(cv, 94, v, label);
    if (d->eta_s == 0u) {                                     /* 0 = unknown (PROTOCOL.md) */
        buf[0] = '\0';
        (void)rt_fmt_cat(buf, sizeof buf, "--:--");
    } else {
        rt_fmt_hhmm(d->eta_s, buf, sizeof buf);
    }
    column(cv, 150, buf, "ETA");
    rt_fmt_distance(d->remaining_m, v, u);
    buf[0] = '\0';
    (void)rt_fmt_cat(buf, sizeof buf, u);
    (void)rt_fmt_cat(buf, sizeof buf, " LEFT");
    column(cv, 206, v, buf);
}

/* The time now, small, left of the battery and signal (like a phone's status bar). */
static void draw_clock(RtCanvas *cv, const RtHudData *d)
{
    if (!d->have_clock) {
        return;
    }
    char hhmm[8];
    rt_fmt_hhmm(d->time_s, hhmm, sizeof hhmm);
    int32_t right = (d->status.known && d->status.google_cap) ? 176 : 192;
    (void)rt_text(cv, right - rt_text_width(hhmm, 1), 11, hhmm, 1, RT_C_TEXT, false);
}

void rt_draw_hud(RtCanvas *cv, const RtTheme *t, const RtHudData *d, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && d != NULL)) {
        return;
    }
    char buf[64];
    draw_next(cv, t, d, now_ms);
    draw_clock(cv, d);
    draw_secondary(cv, t, d, now_ms);
    if (d->current_road != NULL && d->current_road[0] != '\0') {   /* Pokemon location sign */
        rt_chip(cv, t, 120, 240, rt_fit(d->current_road, 1, 200, buf, sizeof buf), RT_C_TEXT);
    }
    draw_bottom(cv, t, d);
}
