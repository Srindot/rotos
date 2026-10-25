/*
 * rt_widgets.c - panels, chips, status indicator.
 */
#include "rt_widgets.h"

#include "rn/rn_text.h"
#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"

static const uint8_t BAYER4[4][4] = {{0u, 8u, 2u, 10u}, {12u, 4u, 14u, 6u}, {3u, 11u, 1u, 9u}, {15u, 7u, 13u, 5u}};

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} Box;

/* Border pixel: Hyprland's ACCENT -> HILITE diagonal gradient (ordered
 * dither, two colours) or a flat colour for the Pokemon style. */
static void border_px(RtCanvas *cv, const RtTheme *t, const Box *outer, int32_t i, int32_t j, uint8_t flat)
{
    if (t->panel == (uint8_t)RT_PANEL_GRADIENT) {
        int32_t lvl = (i - outer->x + j - outer->y) * 16 / (outer->w + outer->h);
        uint8_t th = BAYER4[j & 3][i & 3];
        rt_canvas_pixel(cv, i, j, (lvl > (int32_t)th) ? RT_C_HILITE : RT_C_ACCENT);
    } else {
        rt_canvas_pixel(cv, i, j, flat);
    }
}

/* Rounded 1 px outline of box b. */
static void outline(RtCanvas *cv, const RtTheme *t, const Box *outer, const Box *b, uint8_t col)
{
    for (int32_t i = b->x + 2; i < b->x + b->w - 2; i++) {
        border_px(cv, t, outer, i, b->y, col);
        border_px(cv, t, outer, i, b->y + b->h - 1, col);
    }
    for (int32_t j = b->y + 2; j < b->y + b->h - 2; j++) {
        border_px(cv, t, outer, b->x, j, col);
        border_px(cv, t, outer, b->x + b->w - 1, j, col);
    }
    border_px(cv, t, outer, b->x + 1, b->y + 1, col);
    border_px(cv, t, outer, b->x + b->w - 2, b->y + 1, col);
    border_px(cv, t, outer, b->x + 1, b->y + b->h - 2, col);
    border_px(cv, t, outer, b->x + b->w - 2, b->y + b->h - 2, col);
}

void rt_panel(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h, const RtTheme *t, bool glass)
{
    if (!RN_ASSERT(cv != NULL && t != NULL) || !RN_ASSERT(w >= 6 && h >= 6)) {
        return;
    }
    if (glass) {
        rt_canvas_glass(cv, x + 1, y + 1, w - 2, h - 2);     /* map shows through */
    } else {
        rt_canvas_fill_rect(cv, x + 2, y, w - 4, h, RT_C_PANEL);
        rt_canvas_fill_rect(cv, x, y + 2, w, h - 4, RT_C_PANEL);
        rt_canvas_fill_rect(cv, x + 1, y + 1, w - 2, h - 2, RT_C_PANEL);
    }
    Box outer = {x, y, w, h};
    if (t->panel == (uint8_t)RT_PANEL_GRADIENT) {
        outline(cv, t, &outer, &outer, RT_C_ACCENT);
    } else {
        Box inner = {x + 2, y + 2, w - 4, h - 4};
        outline(cv, t, &outer, &outer, RT_C_SHADOW);
        outline(cv, t, &outer, &inner, RT_C_ACCENT);
    }
}

void rt_chip(RtCanvas *cv, const RtTheme *t, int32_t cx, int32_t y, const char *text, uint8_t color)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && text != NULL)) {
        return;
    }
    int32_t w = rt_text_width(text, 1) + 12;
    rt_panel(cv, cx - w / 2, y, w, 15, t, false);
    (void)rt_text(cv, cx - w / 2 + 6, y + 4, text, 1, color, false);
}

static void draw_battery(RtCanvas *cv, int32_t bx, int32_t y, const RtStatusView *s, bool blink)
{
    uint8_t edge = s->known ? RT_C_TEXT : RT_C_TEXT_DIM;
    rt_canvas_rect(cv, bx, y, 13, 7, edge);
    rt_canvas_fill_rect(cv, bx + 13, y + 2, 2, 3, edge);
    if (!s->known) {
        return;
    }
    int32_t fill = ((int32_t)s->battery * 11 + 50) / 100;
    bool low = s->battery < 15u;
    if (!(low && blink)) {
        uint8_t col = s->charging ? RT_C_ACCENT : (low ? RT_C_HILITE : RT_C_TEXT);
        rt_canvas_fill_rect(cv, bx + 1, y + 1, fill, 5, col);
    }
}

void rt_status(RtCanvas *cv, int32_t right_x, int32_t y, const RtStatusView *s, bool with_pct, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && s != NULL)) {
        return;
    }
    bool blink = ((now_ms / 500u) & 1u) != 0u;
    int32_t bx = right_x - 15;
    draw_battery(cv, bx, y, s, blink);
    int32_t x = bx - 4;
    if (with_pct && s->known) {
        char pct[8] = "";
        (void)rt_fmt_u32(pct, sizeof pct, s->battery, 1u);
        (void)rt_fmt_cat(pct, sizeof pct, "%");
        x -= rt_text_width(pct, 1);
        (void)rt_text(cv, x, y, pct, 1, RT_C_TEXT, false);
        x -= 6;
    }
    int32_t sx = x - 11;                       /* 4 signal bars */
    for (int32_t i = 0; i < 4; i++) {
        int32_t h = 2 + i * 5 / 3 + ((i == 3) ? 1 : 0);
        bool lit = s->known && s->link_ok && (uint32_t)i < s->bars;
        rt_canvas_fill_rect(cv, sx + i * 3, y + 7 - h, 2, h, lit ? RT_C_TEXT : RT_C_ROUTE_DONE);
    }
    if (s->known && s->google_cap) {         /* phone near its Google usage cap */
        (void)rt_text(cv, sx - 4 - rt_text_width("G!", 1), y, "G!", 1, RT_C_HILITE, false);
    }
    if (s->known && !s->link_ok && blink) {   /* link lost: blinking X */
        for (int32_t k = 0; k < 5; k++) {
            rt_canvas_pixel(cv, sx + 3 + k, y + 1 + k, RT_C_HILITE);
            rt_canvas_pixel(cv, sx + 7 - k, y + 1 + k, RT_C_HILITE);
        }
    }
}

const char *rt_fit(const char *s, int32_t scale, int32_t max_w, char *out, uint32_t cap)
{
    if (!RN_ASSERT(out != NULL && cap > 3u) || !RN_ASSERT(s != NULL)) {
        return "";
    }
    uint32_t n = rn_text_copy(out, cap, s);
    if (rt_text_width(out, scale) <= max_w) {
        return out;
    }
    while (n > 0u) {
        n--;
        out[n] = '\0';
        char tmp[64] = "";
        (void)rt_fmt_cat(tmp, sizeof tmp, out);
        (void)rt_fmt_cat(tmp, sizeof tmp, "..");
        if (rt_text_width(tmp, scale) <= max_w) {
            (void)rn_text_copy(out, cap, tmp);
            break;
        }
    }
    return out;
}

void rt_dot_grid(RtCanvas *cv, int32_t phase)
{
    if (!RN_ASSERT(cv != NULL) || !RN_ASSERT(phase >= 0 && phase < 8)) {
        return;
    }
    for (int32_t y = 4 + phase; y < RT_SCREEN_H; y += 8) {
        for (int32_t x = 4; x < RT_SCREEN_W; x += 8) {
            rt_canvas_pixel(cv, x, y, RT_C_BUILDING);
        }
    }
}
