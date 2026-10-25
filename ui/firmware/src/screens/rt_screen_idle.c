/*
 * rt_screen_idle.c - the screen while no phone is connected: ROTOS's mascot
 * in the middle of a radar that keeps pulsing outwards ("looking for your
 * phone"), what to do to connect, and the stored riding statistics in the
 * same bar style as the riding screen.
 */
#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_icons.h"
#include "rt_screens.h"
#include "rt_sprites.h"

#define RADAR_X    120
#define RADAR_Y    98
#define RADAR_MIN  38
#define RADAR_SPAN 40
#define RINGS      3
#define STEPS_Y    212
#define STATS_Y    260

/* A circle outline (midpoint algorithm), 2 px thick. */
static void ring(RtCanvas *cv, int32_t cx, int32_t cy, int32_t r, uint8_t c)
{
    for (int32_t t = 0; t < 2; t++) {
        int32_t x = r - t;
        int32_t y = 0;
        int32_t err = 1 - x;
        for (int32_t guard = 0; guard < 4 * RADAR_SPAN && x >= y; guard++) {
            const int32_t px[8] = {x, y, -y, -x, -x, -y, y, x};
            const int32_t py[8] = {y, x, x, y, -y, -x, -x, -y};
            for (int32_t k = 0; k < 8; k++) {
                rt_canvas_pixel(cv, cx + px[k], cy + py[k], c);
            }
            y++;
            if (err < 0) {
                err += 2 * y + 1;
            } else {
                x--;
                err += 2 * (y - x) + 1;
            }
        }
    }
}

/* Rings pulsing out from the mascot, brightest near it, fading at the edge. */
static void draw_radar(RtCanvas *cv, uint32_t ms)
{
    uint32_t phase = (ms / 16u) % (uint32_t)RADAR_SPAN;
    for (int32_t k = 0; k < RINGS; k++) {
        int32_t d = (int32_t)((phase + (uint32_t)(k * RADAR_SPAN / RINGS)) % (uint32_t)RADAR_SPAN);
        uint8_t c = (d < RADAR_SPAN / 3) ? RT_C_ACCENT : (d < 2 * RADAR_SPAN / 3) ? RT_C_ROUTE : RT_C_TEXT_DIM;
        ring(cv, RADAR_X, RADAR_Y, RADAR_MIN + d, c);
    }
    rt_canvas_fill_circle(cv, RADAR_X, RADAR_Y, RADAR_MIN - 4, RT_C_PANEL);
    ring(cv, RADAR_X, RADAR_Y, RADAR_MIN - 4, RT_C_ACCENT);
    int32_t bob = (((ms / 500u) & 1u) != 0u) ? 1 : 0;
    uint8_t mood = ((ms / 150u) % 24u == 0u) ? RT_MOOD_BLINK : RT_MOOD_IDLE;
    rt_icon_mascot(cv, RADAR_X - 32, RADAR_Y - 34 + bob, mood, 4);
}

static void draw_title(RtCanvas *cv, uint32_t ms)
{
    const char *title = "SEARCHING";
    int32_t w = rt_text_width(title, 2);
    int32_t x = RADAR_X - w / 2 - 9;
    (void)rt_text(cv, x, 180, title, 2, RT_C_TEXT, false);
    uint32_t dots = (ms / 400u) % 4u;
    for (uint32_t i = 0u; i < dots; i++) {
        rt_canvas_fill_rect(cv, x + w + 4 + (int32_t)i * 6, 192, 3, 3, RT_C_ACCENT);
    }
    (void)rt_text_centered(cv, 120, 199, "FOR YOUR PHONE", 1, RT_C_TEXT_DIM, false);
}

/* Two numbered steps to connect. */
static void draw_steps(RtCanvas *cv, const RtTheme *t)
{
    rt_panel(cv, 6, STEPS_Y, 228, 42, t, false);
    rt_canvas_fill_circle(cv, 20, STEPS_Y + 12, 6, RT_C_ACCENT);
    (void)rt_text_centered(cv, 21, STEPS_Y + 9, "1", 1, RT_C_PANEL, false);
    (void)rt_text(cv, 32, STEPS_Y + 9, "OPEN THE ROTOS APP", 1, RT_C_TEXT, false);
    rt_canvas_fill_circle(cv, 20, STEPS_Y + 29, 6, RT_C_ACCENT);
    (void)rt_text_centered(cv, 21, STEPS_Y + 26, "2", 1, RT_C_PANEL, false);
    (void)rt_text(cv, 32, STEPS_Y + 26, "SWITCH ON CONNECT TO ROTOS", 1, RT_C_TEXT, false);
}

/* One stat column: big value, small label under it. */
static void stat(RtCanvas *cv, int32_t cx, const char *value, const char *label)
{
    (void)rt_text_centered(cv, cx, STATS_Y + 12, value, 2, RT_C_TEXT, false);
    (void)rt_text_centered(cv, cx, STATS_Y + 37, label, 1, RT_C_TEXT_DIM, false);
}

/* Odometer, rides and the last ride, like the riding screen's bottom bar. */
static void draw_stats(RtCanvas *cv, const RtTheme *t, const RtIdleView *d)
{
    rt_panel(cv, 6, STATS_Y, 228, 54, t, false);
    if (d->rides == 0u) {
        (void)rt_text_centered(cv, 120, STATS_Y + 14, "FIRST RIDE AHEAD", 2, RT_C_TEXT, false);
        (void)rt_text_centered(cv, 120, STATS_Y + 37, "YOUR RIDES WILL SHOW UP HERE", 1, RT_C_TEXT_DIM, false);
        return;
    }
    for (int32_t y = STATS_Y + 8; y < STATS_Y + 46; y += 2) {
        rt_canvas_pixel(cv, 82, y, RT_C_TEXT_DIM);
        rt_canvas_pixel(cv, 158, y, RT_C_TEXT_DIM);
    }
    char v[RT_VALUE_LEN];
    char u[RT_UNIT_LEN];
    char label[16] = "";
    v[0] = '\0';
    (void)rt_fmt_u32(v, sizeof v, d->odo_m / 1000u, 1u);
    stat(cv, 44, v, "KM TOTAL");
    v[0] = '\0';
    (void)rt_fmt_u32(v, sizeof v, d->rides, 1u);
    stat(cv, 120, v, (d->rides == 1u) ? "RIDE" : "RIDES");
    rt_fmt_distance((float)d->last_m, v, u);
    (void)rt_fmt_cat(label, sizeof label, "LAST ");
    (void)rt_fmt_cat(label, sizeof label, u);
    stat(cv, 196, v, label);
}

void rt_draw_waiting(RtCanvas *cv, const RtTheme *t, const RtIdleView *d, uint32_t ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && d != NULL)) {
        return;
    }
    rt_canvas_clear(cv, RT_C_LAND);
    rt_dot_grid(cv, 0);
    (void)rt_text(cv, 10, 6, "ROTOS", 1, RT_C_TEXT_DIM, false);
    (void)rt_text(cv, 230 - rt_text_width("B: THEME", 1), 6, "B: THEME", 1, RT_C_TEXT_DIM, false);
    draw_radar(cv, ms);
    draw_title(cv, ms);
    draw_steps(cv, t);
    draw_stats(cv, t, d);
}
