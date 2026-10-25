/*
 * ht_screen.c - hardware test screens, drawn into the 4-bit canvas.
 */
#include "ht_screen.h"

#include <stdio.h>

#include "ht_palette.h"
#include "rn/rn_types.h"
#include "rt_font.h"

#define BAR_COUNT 8
#define BAR_W     28
#define BAR_Y     40
#define BAR_H     48
#define BOX_W     70
#define BOX_H     96
#define BOX_Y     124
#define BOX_X0    10
#define BOX_STEP  76

static const uint8_t BAR_COLOR[BAR_COUNT] = {HT_RED, HT_GREEN, HT_BLUE, HT_WHITE,
                                             HT_YELLOW, HT_CYAN, HT_MAGENTA, HT_GREY};
static const char *const BAR_NAME[BAR_COUNT] = {"R", "G", "B", "W", "Y", "C", "M", "-"};
static const char *const BOX_NAME[HAL_BUTTON_COUNT] = {"A", "B", "C"};

void ht_screen_sweep(RtCanvas *cv, uint32_t step)
{
    if (!RN_ASSERT(cv != NULL && step < HT_SWEEP_COUNT)) {
        return;
    }
    uint8_t bg = ht_sweep_color(step);
    uint8_t fg = (bg == HT_WHITE || bg == HT_YELLOW) ? HT_BLACK : HT_WHITE;
    rt_canvas_begin(cv);
    rt_canvas_clear(cv, bg);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, 130, ht_sweep_name(step), 5, fg, false);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, 180, "SCREEN SHOULD BE THIS COLOUR", 1, fg, false);
}

static void draw_bars(RtCanvas *cv)
{
    int32_t x0 = (RT_SCREEN_W - BAR_COUNT * BAR_W) / 2;
    for (int32_t i = 0; i < BAR_COUNT; i++) {
        int32_t x = x0 + i * BAR_W;
        rt_canvas_fill_rect(cv, x, BAR_Y, BAR_W, BAR_H, BAR_COLOR[i]);
        (void)rt_text_centered(cv, x + BAR_W / 2, BAR_Y + BAR_H + 4, BAR_NAME[i], 1, HT_WHITE, false);
    }
}

static void draw_box(RtCanvas *cv, const HtButtons *b, uint32_t i)
{
    int32_t x = BOX_X0 + (int32_t)i * BOX_STEP;
    bool down = ht_buttons_down(b, i);
    rt_canvas_fill_rect(cv, x, BOX_Y, BOX_W, BOX_H, down ? HT_GREEN : HT_DARK);
    rt_canvas_rect(cv, x, BOX_Y, BOX_W, BOX_H, down ? HT_WHITE : HT_GREY);
    uint8_t fg = down ? HT_BLACK : HT_WHITE;
    (void)rt_text_centered(cv, x + BOX_W / 2, BOX_Y + 18, BOX_NAME[i], 5, fg, false);
    (void)rt_text_centered(cv, x + BOX_W / 2, BOX_Y + 66, down ? "DOWN" : "UP", 2, fg, false);
    char n[16];
    (void)snprintf(n, sizeof n, "X%lu", (unsigned long)b->presses[i]);
    (void)rt_text_centered(cv, x + BOX_W / 2, BOX_Y + BOX_H + 8, n, 2, HT_YELLOW, false);
}

void ht_screen_test(RtCanvas *cv, const HtButtons *b, uint32_t uptime_s, uint32_t fps)
{
    if (!RN_ASSERT(cv != NULL && b != NULL)) {
        return;
    }
    rt_canvas_begin(cv);
    rt_canvas_clear(cv, HT_BLACK);
    rt_canvas_rect(cv, 0, 0, RT_SCREEN_W, RT_SCREEN_H, HT_WHITE);     /* all 4 edges visible */
    rt_canvas_rect(cv, 2, 2, RT_SCREEN_W - 4, RT_SCREEN_H - 4, HT_RED);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, 12, "ROTOS HW TEST", 2, HT_WHITE, false);
    draw_bars(cv);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, BOX_Y - 14, "PRESS EACH BUTTON", 1, HT_CYAN, false);
    for (uint32_t i = 0u; i < HAL_BUTTON_COUNT; i++) {
        draw_box(cv, b, i);
    }
    char line[32];
    (void)snprintf(line, sizeof line, "UP %lu S  FPS %lu", (unsigned long)uptime_s, (unsigned long)fps);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, 270, line, 2, HT_WHITE, false);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, 296, "TOP EDGE IS THE TITLE", 1, HT_GREY, false);
}

#define GRID 40

static void draw_quadrants(RtCanvas *cv)
{
    static const uint8_t QUAD[4] = {HT_RED, HT_GREEN, HT_BLUE, HT_YELLOW};
    static const char *const NAME[4][2] = {{"TOP", "LEFT"}, {"TOP", "RIGHT"}, {"BOTTOM", "LEFT"}, {"BOTTOM", "RIGHT"}};
    int32_t hw = RT_SCREEN_W / 2;
    int32_t hh = RT_SCREEN_H / 2;
    for (int32_t q = 0; q < 4; q++) {
        int32_t x = (q % 2) * hw;
        int32_t y = (q / 2) * hh;
        uint8_t fg = (QUAD[q] == HT_YELLOW || QUAD[q] == HT_GREEN) ? HT_BLACK : HT_WHITE;
        rt_canvas_fill_rect(cv, x, y, hw, hh, QUAD[q]);
        (void)rt_text_centered(cv, x + hw / 2, y + hh / 2 - 22, NAME[q][0], 2, fg, false);
        (void)rt_text_centered(cv, x + hw / 2, y + hh / 2 - 4, NAME[q][1], 2, fg, false);
    }
}

static void draw_grid(RtCanvas *cv)
{
    char n[8];
    for (int32_t x = GRID; x < RT_SCREEN_W; x += GRID) {
        rt_canvas_fill_rect(cv, x, 0, 1, RT_SCREEN_H, HT_WHITE);
        (void)snprintf(n, sizeof n, "%ld", (long)x);
        (void)rt_text(cv, x + 2, 3, n, 1, HT_BLACK, false);
    }
    for (int32_t y = GRID; y < RT_SCREEN_H; y += GRID) {
        rt_canvas_hline(cv, 0, RT_SCREEN_W - 1, y, HT_WHITE);
        (void)snprintf(n, sizeof n, "%ld", (long)y);
        (void)rt_text(cv, 3, y + 2, n, 1, HT_BLACK, false);
    }
    rt_canvas_rect(cv, 0, 0, RT_SCREEN_W, RT_SCREEN_H, HT_MAGENTA);
    rt_canvas_rect(cv, 1, 1, RT_SCREEN_W - 2, RT_SCREEN_H - 2, HT_MAGENTA);
}

static void draw_arrow(RtCanvas *cv)
{
    const float xs[3] = {120.0f, 96.0f, 144.0f};
    const float ys[3] = {124.0f, 152.0f, 152.0f};
    rt_canvas_fill_rect(cv, 84, 122, 72, 76, HT_BLACK);
    rt_canvas_fill_triangle(cv, xs, ys, HT_WHITE);
    rt_canvas_fill_rect(cv, 114, 152, 12, 22, HT_WHITE);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, 178, "UP", 2, HT_WHITE, false);
}

void ht_screen_chart(RtCanvas *cv, uint32_t mode, uint8_t madctl)
{
    if (!RN_ASSERT(cv != NULL)) {
        return;
    }
    rt_canvas_begin(cv);
    draw_quadrants(cv);
    draw_grid(cv);
    draw_arrow(cv);
    char line[24];
    (void)snprintf(line, sizeof line, "MODE %lu  %02X", (unsigned long)mode, (unsigned)madctl);
    rt_canvas_fill_rect(cv, 60, 200, 120, 40, HT_BLACK);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, 204, line, 2, HT_WHITE, false);
    (void)rt_text_centered(cv, RT_SCREEN_W / 2, 226, "B = NEXT MODE", 1, HT_CYAN, false);
}
