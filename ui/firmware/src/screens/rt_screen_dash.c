/*
 * rt_screen_dash.c - dashboard: mascot, big clock, date, weather, phone status.
 */
#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_icons.h"
#include "rt_screens.h"
#include "rt_sprites.h"

static const char *const DAYS[7] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
static const char *const MONTHS[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                       "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

static void draw_clock(RtCanvas *cv, const RtDashData *d)
{
    char buf[24] = "";
    if (!d->have_clock) {
        (void)rt_text_centered(cv, 120, 112, "--:--", 6, RT_C_TEXT_DIM, false);
        return;
    }
    rt_fmt_hhmm(d->time_s, buf, sizeof buf);
    (void)rt_text_centered(cv, 120, 112, buf, 6, RT_C_TEXT, false);
    buf[0] = '\0';
    (void)rt_fmt_cat(buf, sizeof buf, DAYS[d->weekday % 7u]);
    (void)rt_fmt_cat(buf, sizeof buf, " ");
    char day[4] = "";
    (void)rt_fmt_u32(day, sizeof day, d->day, 2u);
    (void)rt_fmt_cat(buf, sizeof buf, day);
    (void)rt_fmt_cat(buf, sizeof buf, " ");
    (void)rt_fmt_cat(buf, sizeof buf, MONTHS[((uint32_t)d->month + 11u) % 12u]);
    (void)rt_text_centered(cv, 120, 162, buf, 2, RT_C_TEXT_DIM, false);
}

static void draw_weather(RtCanvas *cv, const RtTheme *t, const RtDashData *d)
{
    rt_panel(cv, 12, 190, 216, 74, t, true);
    if (!d->have_weather) {
        (void)rt_text_centered(cv, 120, 224, "NO WEATHER DATA", 1, RT_C_TEXT_DIM, false);
        return;
    }
    const RnWeather *w = &d->weather;
    char buf[40] = "";
    rt_icon_weather(cv, 22, 203, w->code, w->is_day, 3);
    (void)rt_fmt_round(buf, sizeof buf, w->temp_c);
    (void)rt_fmt_cat(buf, sizeof buf, "~C");
    (void)rt_text(cv, 82, 200, buf, 4, RT_C_TEXT, false);
    (void)rt_text(cv, 82, 236, rt_weather_name(w->code), 1, RT_C_ACCENT, false);
    char n[8] = "";
    buf[0] = '\0';
    (void)rt_fmt_cat(buf, sizeof buf, "RAIN ");
    (void)rt_fmt_u32(n, sizeof n, w->rain_prob, 1u);
    (void)rt_fmt_cat(buf, sizeof buf, n);
    (void)rt_fmt_cat(buf, sizeof buf, "%  WIND ");
    (void)rt_fmt_u32(n, sizeof n, w->wind_kmh, 1u);
    (void)rt_fmt_cat(buf, sizeof buf, n);
    (void)rt_fmt_cat(buf, sizeof buf, " KM/H");
    (void)rt_text(cv, 82, 248, buf, 1, RT_C_TEXT_DIM, false);
}

void rt_draw_dashboard(RtCanvas *cv, const RtTheme *t, const RtDashData *d, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && d != NULL)) {
        return;
    }
    rt_canvas_clear(cv, RT_C_LAND);
    rt_dot_grid(cv, 0);
    (void)rt_text(cv, 10, 10, "ROTOS", 1, RT_C_TEXT_DIM, false);
    rt_status(cv, 230, 10, &d->status, true, now_ms);
    int32_t bob = (((now_ms / 600u) & 1u) != 0u) ? 2 : 0;      /* gentle bob */
    uint8_t mood = ((now_ms % 4000u) < 160u) ? RT_MOOD_BLINK : RT_MOOD_IDLE;
    rt_icon_mascot(cv, 88, 34 + bob, mood, 4);
    draw_clock(cv, d);
    draw_weather(cv, t, d);
    const char *foot = d->footer;
    if (foot == NULL) {
        foot = !d->status.known ? "WAITING FOR PHONE"
               : (d->status.link_ok ? "READY - START A ROUTE ON YOUR PHONE" : "PHONE LINK LOST");
    }
    (void)rt_text_centered(cv, 120, 292, foot, 1, d->status.link_ok ? RT_C_TEXT_DIM : RT_C_HILITE, false);
}
