/*
 * rt_screen_summary.c - "RIDE COMPLETE" card over the frozen map.
 */
#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_icons.h"
#include "rt_screens.h"
#include "rt_sprites.h"

static int32_t row(RtCanvas *cv, int32_t y, const char *label, const char *value, const char *unit)
{
    (void)rt_text(cv, 24, y + 4, label, 1, RT_C_TEXT_DIM, false);
    int32_t wu = (unit[0] != '\0') ? rt_text_width(unit, 1) + 4 : 0;
    int32_t x = 216 - wu - rt_text_width(value, 2);
    int32_t end = rt_text(cv, x, y, value, 2, RT_C_TEXT, false);
    if (wu > 0) {
        (void)rt_text(cv, end + 4, y + 7, unit, 1, RT_C_TEXT_DIM, false);
    }
    for (int32_t i = 24; i < 216; i += 2) {                  /* dotted rule */
        rt_canvas_pixel(cv, i, y + 19, RT_C_ROUTE_DONE);
    }
    return y + 26;
}

void rt_draw_summary(RtCanvas *cv, const RtTheme *t, const RtRideSummary *r, uint32_t seconds_left)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && r != NULL)) {
        return;
    }
    char v[RT_VALUE_LEN + 8];
    char u[RT_UNIT_LEN];
    rt_panel(cv, 10, 24, 220, 272, t, true);
    (void)rt_text_centered(cv, 120, 36, "RIDE COMPLETE", 2, RT_C_TEXT, false);
    rt_icon_mascot(cv, 96, 56, RT_MOOD_HAPPY, 3);
    int32_t y = 112;
    rt_fmt_distance(r->distance_m, v, u);
    y = row(cv, y, "DISTANCE", v, u);
    rt_fmt_duration(r->ride_s, v, sizeof v);
    y = row(cv, y, "RIDE TIME", v, "");
    rt_fmt_duration(r->moving_s, v, sizeof v);
    y = row(cv, y, "MOVING", v, "");
    (void)rt_fmt_round(v, sizeof v, r->avg_kmh);
    y = row(cv, y, "AVG SPEED", v, "KM/H");
    (void)rt_fmt_round(v, sizeof v, r->max_kmh);
    y = row(cv, y, "TOP SPEED", v, "KM/H");
    if (r->have_clock) {
        char b[8];
        rt_fmt_hhmm(r->start_s, v, sizeof v);
        (void)rt_fmt_cat(v, sizeof v, "-");
        rt_fmt_hhmm(r->end_s, b, sizeof b);
        (void)rt_fmt_cat(v, sizeof v, b);
        (void)row(cv, y, "TIME", v, "");
    }
    char foot[32] = "DASHBOARD IN ";
    char n[8] = "";
    (void)rt_fmt_u32(n, sizeof n, seconds_left, 1u);
    (void)rt_fmt_cat(foot, sizeof foot, n);
    (void)rt_text_centered(cv, 120, 282, foot, 1, RT_C_TEXT_DIM, false);
}
