/*
 * rt_screen_weather.c - weather page: now, the next six hours, sunrise and
 * sunset. Monochrome Waybar-style icons; only rain is highlighted.
 */
#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_icons.h"
#include "rt_screens.h"

#define HOURS_SHOWN 6u
#define COL_W       36
#define RAIN_ALERT  50u

void rt_draw_page_dots(RtCanvas *cv, uint32_t n, uint32_t current)
{
    if (!RN_ASSERT(cv != NULL) || !RN_ASSERT(n > 0u && n <= 5u)) {
        return;
    }
    int32_t x0 = 120 - (int32_t)(n * 8u) / 2;
    for (uint32_t i = 0u; i < n; i++) {
        int32_t x = x0 + (int32_t)i * 8;
        rt_canvas_rect(cv, x, 308, 5, 5, RT_C_TEXT_DIM);
        if (i == current) {
            rt_canvas_fill_rect(cv, x, 308, 5, 5, RT_C_ACCENT);
        }
    }
}

static void draw_now(RtCanvas *cv, const RtWeatherPage *w)
{
    if (!w->have_weather) {
        (void)rt_text_centered(cv, 120, 60, "NO WEATHER DATA", 1, RT_C_TEXT_DIM, false);
        return;
    }
    char buf[40] = "";
    char n[8] = "";
    rt_icon_weather(cv, 14, 30, w->weather.code, w->weather.is_day && !w->night, 4);
    (void)rt_fmt_round(buf, sizeof buf, w->weather.temp_c);
    (void)rt_fmt_cat(buf, sizeof buf, "~C");
    (void)rt_text(cv, 92, 34, buf, 5, RT_C_TEXT, false);
    (void)rt_text(cv, 92, 76, rt_weather_name(w->weather.code), 1, RT_C_ACCENT, false);
    buf[0] = '\0';
    (void)rt_fmt_cat(buf, sizeof buf, "RAIN ");
    (void)rt_fmt_u32(n, sizeof n, w->weather.rain_prob, 1u);
    (void)rt_fmt_cat(buf, sizeof buf, n);
    (void)rt_fmt_cat(buf, sizeof buf, "%  WIND ");
    (void)rt_fmt_u32(n, sizeof n, w->weather.wind_kmh, 1u);
    (void)rt_fmt_cat(buf, sizeof buf, n);
    (void)rt_fmt_cat(buf, sizeof buf, " KM/H");
    (void)rt_text(cv, 92, 90, buf, 1, RT_C_TEXT_DIM, false);
}

static bool hour_is_day(const RnForecast *f, uint32_t hour)
{
    uint32_t mid = hour * 60u + 30u;
    return mid >= f->sunrise_min && mid < f->sunset_min;
}

static void draw_hour(RtCanvas *cv, const RnForecast *f, uint32_t i)
{
    const RnForecastHour *h = &f->hours[i];
    uint32_t hour = ((uint32_t)f->start_hour + i) % 24u;
    int32_t cx = 12 + COL_W / 2 + (int32_t)i * COL_W;
    char buf[12] = "";
    (void)rt_fmt_u32(buf, sizeof buf, hour, 2u);
    (void)rt_text_centered(cv, cx, 130, buf, 1, RT_C_TEXT_DIM, false);
    rt_icon_weather(cv, cx - 8, 142, h->code, hour_is_day(f, hour), 1);
    buf[0] = '\0';
    (void)rt_fmt_round(buf, sizeof buf, h->temp_c);
    (void)rt_fmt_cat(buf, sizeof buf, "~");
    (void)rt_text_centered(cv, cx, 164, buf, 1, RT_C_TEXT, false);
    buf[0] = '\0';
    (void)rt_fmt_u32(buf, sizeof buf, h->rain_prob, 1u);
    (void)rt_fmt_cat(buf, sizeof buf, "%");
    bool wet = h->rain_prob >= RAIN_ALERT;
    (void)rt_text_centered(cv, cx, 178, buf, 1, wet ? RT_C_HILITE : RT_C_TEXT_DIM, false);
    rt_canvas_rect(cv, cx - 12, 194, 24, 6, RT_C_ROUTE_DONE);         /* rain bar */
    int32_t fill = (int32_t)h->rain_prob * 22 / 100;
    rt_canvas_fill_rect(cv, cx - 11, 195, fill, 4, wet ? RT_C_HILITE : RT_C_ACCENT);
}

static void draw_hours(RtCanvas *cv, const RtTheme *t, const RtWeatherPage *w)
{
    rt_panel(cv, 8, 108, 224, 104, t, true);
    (void)rt_text(cv, 16, 114, "NEXT HOURS", 1, RT_C_TEXT_DIM, false);
    if (!w->have_forecast || w->forecast.n == 0u) {
        (void)rt_text_centered(cv, 120, 156, "NO FORECAST YET", 1, RT_C_TEXT_DIM, false);
        return;
    }
    uint32_t n = (w->forecast.n < HOURS_SHOWN) ? w->forecast.n : HOURS_SHOWN;
    for (uint32_t i = 0u; i < n; i++) {
        draw_hour(cv, &w->forecast, i);
    }
}

/* "SUNSET IN 1H 20M": the next sun event after now. */
static void sun_countdown(const RtWeatherPage *w, char *out, uint32_t cap)
{
    uint32_t now = (w->time_s / 60u) % 1440u;
    const RnForecast *f = &w->forecast;
    bool before_rise = now < f->sunrise_min;
    bool before_set = !before_rise && now < f->sunset_min;
    uint32_t target = before_set ? f->sunset_min : f->sunrise_min;
    uint32_t mins = (target + 1440u - now) % 1440u;
    out[0] = '\0';
    (void)rt_fmt_cat(out, cap, before_set ? "SUNSET IN " : "SUNRISE IN ");
    char d[8] = "";
    if (mins >= 60u) {
        (void)rt_fmt_u32(d, sizeof d, mins / 60u, 1u);
        (void)rt_fmt_cat(out, cap, d);
        (void)rt_fmt_cat(out, cap, "H ");
        (void)rt_fmt_u32(d, sizeof d, mins % 60u, 2u);
        (void)rt_fmt_cat(out, cap, d);
        (void)rt_fmt_cat(out, cap, "M");
    } else {
        (void)rt_fmt_u32(d, sizeof d, mins, 1u);
        (void)rt_fmt_cat(out, cap, d);
        (void)rt_fmt_cat(out, cap, " MIN");
    }
}

static void draw_sun(RtCanvas *cv, const RtTheme *t, const RtWeatherPage *w)
{
    if (!w->have_forecast) {
        return;
    }
    char buf[32] = "";
    char hm[8] = "";
    rt_panel(cv, 8, 220, 224, 48, t, true);
    (void)rt_fmt_cat(buf, sizeof buf, "SUNRISE ");
    rt_fmt_hhmm(w->forecast.sunrise_min * 60u, hm, sizeof hm);
    (void)rt_fmt_cat(buf, sizeof buf, hm);
    (void)rt_text(cv, 18, 230, buf, 1, RT_C_TEXT_DIM, false);
    buf[0] = '\0';
    (void)rt_fmt_cat(buf, sizeof buf, "SUNSET ");
    rt_fmt_hhmm(w->forecast.sunset_min * 60u, hm, sizeof hm);
    (void)rt_fmt_cat(buf, sizeof buf, hm);
    (void)rt_text(cv, 222 - rt_text_width(buf, 1), 230, buf, 1, RT_C_TEXT_DIM, false);
    if (w->have_clock) {
        sun_countdown(w, buf, sizeof buf);
        (void)rt_text_centered(cv, 120, 248, buf, 1, RT_C_ACCENT, false);
    }
}

void rt_draw_weather(RtCanvas *cv, const RtTheme *t, const RtWeatherPage *w, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && w != NULL)) {
        return;
    }
    rt_canvas_clear(cv, RT_C_LAND);
    rt_dot_grid(cv, 0);
    (void)rt_text(cv, 10, 10, "WEATHER", 1, RT_C_TEXT_DIM, false);
    rt_status(cv, 230, 10, &w->status, true, now_ms);
    draw_now(cv, w);
    draw_hours(cv, t, w);
    draw_sun(cv, t, w);
    if (w->have_forecast) {
        char buf[32] = "UPDATED ";
        char n[8] = "";
        (void)rt_fmt_u32(n, sizeof n, w->forecast_age_s / 60u, 1u);
        (void)rt_fmt_cat(buf, sizeof buf, n);
        (void)rt_fmt_cat(buf, sizeof buf, " MIN AGO");
        (void)rt_text_centered(cv, 120, 284, buf, 1, RT_C_TEXT_DIM, false);
    }
}
