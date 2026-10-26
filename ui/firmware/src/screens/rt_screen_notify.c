/*
 * rt_screen_notify.c - phone notifications: the notifications page (one
 * notification at a time, word-wrapped) and the toast for a new one, a slim
 * bar at the bottom like the call bar (the road ahead stays clear).
 */
#include <string.h>

#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_icons.h"
#include "rt_screens.h"

#define CARD_X 10
#define CARD_Y 34
#define CARD_W 220
#define CARD_H 238
#define LINE_H 14
#define TOAST_X 6
#define TOAST_Y 262
#define TOAST_W 228
#define TOAST_H 52

/* Characters of `s` (from `from`) that fit in max_w at scale 1, broken at a
 * space when possible; *next receives where the following line starts. */
static uint32_t line_len(const char *s, uint32_t from, int32_t max_w, uint32_t *next)
{
    uint32_t per = (uint32_t)(max_w / (rt_text_width("M", 1) + 1));   /* + the 1 px gap after each glyph */
    uint32_t n = (uint32_t)strlen(&s[from]);
    if (n <= per) {
        *next = from + n;
        return n;
    }
    uint32_t cut = per;
    for (uint32_t k = per; k > per / 2u; k--) {
        if (s[from + k] == ' ') {
            cut = k;
            break;
        }
    }
    *next = from + cut + ((s[from + cut] == ' ') ? 1u : 0u);
    return cut;
}

/* Word-wrapped text from (x, y), at most max_lines lines; returns lines drawn. */
static uint32_t draw_wrapped(RtCanvas *cv, int32_t x, int32_t y, int32_t max_w, const char *s,
                             uint32_t max_lines, uint8_t color)
{
    uint32_t pos = 0u;
    uint32_t lines = 0u;
    char buf[RN_NOTIFY_TEXT_MAX + 1u];
    while (lines < max_lines && s[pos] != '\0') {
        uint32_t next = pos;
        uint32_t n = line_len(s, pos, max_w, &next);
        n = (n < RN_NOTIFY_TEXT_MAX) ? n : RN_NOTIFY_TEXT_MAX;
        (void)memcpy(buf, &s[pos], n);
        buf[n] = '\0';
        (void)rt_text(cv, x, y + (int32_t)lines * LINE_H, buf, 1, color, false);
        lines++;
        pos = next;
    }
    return lines;
}

static void draw_age(RtCanvas *cv, uint32_t age_s)
{
    char buf[16] = "";
    if (age_s < 60u) {
        (void)rt_fmt_cat(buf, sizeof buf, "NOW");
    } else {
        uint32_t m = age_s / 60u;
        (void)rt_fmt_u32(buf, sizeof buf, (m < 60u) ? m : m / 60u, 1u);
        (void)rt_fmt_cat(buf, sizeof buf, (m < 60u) ? " MIN" : " H");
    }
    (void)rt_text(cv, CARD_X + CARD_W - 10 - rt_text_width(buf, 1), CARD_Y + 10, buf, 1, RT_C_TEXT_DIM, false);
}

static void draw_card(RtCanvas *cv, const RtTheme *t, const RtNotifyView *v)
{
    rt_panel(cv, CARD_X, CARD_Y, CARD_W, CARD_H, t, false);
    char buf[RN_NOTIFY_TITLE_MAX + 4u];
    (void)rt_text(cv, CARD_X + 10, CARD_Y + 10, v->item->app, 1, RT_C_ACCENT, false);
    draw_age(cv, v->age_s);
    const char *title = (v->item->title[0] != '\0') ? v->item->title : v->item->app;
    bool big = rt_text_width(title, 2) <= CARD_W - 20;
    (void)rt_text(cv, CARD_X + 10, CARD_Y + 28, rt_fit(title, big ? 2 : 1, CARD_W - 20, buf, sizeof buf),
                  big ? 2 : 1, RT_C_TEXT, false);
    for (int32_t x = CARD_X + 10; x < CARD_X + CARD_W - 10; x += 2) {
        rt_canvas_pixel(cv, x, CARD_Y + 52, RT_C_TEXT_DIM);
    }
    (void)draw_wrapped(cv, CARD_X + 10, CARD_Y + 62, CARD_W - 30, v->item->text, 11u, RT_C_TEXT);
}

static void draw_footer(RtCanvas *cv, const RtNotifyView *v)
{
    char pos[16] = "";
    (void)rt_fmt_u32(pos, sizeof pos, v->index + 1u, 1u);
    (void)rt_fmt_cat(pos, sizeof pos, " / ");
    char n[8] = "";
    (void)rt_fmt_u32(n, sizeof n, v->count, 1u);
    (void)rt_fmt_cat(pos, sizeof pos, n);
    (void)rt_text_centered(cv, 120, CARD_Y + CARD_H + 6, pos, 1, RT_C_TEXT, false);
    (void)rt_text_centered(cv, 120, CARD_Y + CARD_H + 20, "B OLDER  C NEWER  HOLD B CLEAR", 1, RT_C_TEXT_DIM,
                           false);
}

void rt_draw_notify_page(RtCanvas *cv, const RtTheme *t, const RtNotifyView *v, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && v != NULL)) {
        return;
    }
    rt_canvas_clear(cv, RT_C_LAND);
    rt_dot_grid(cv, 0);
    (void)rt_text(cv, 10, 10, "NOTIFICATIONS", 1, RT_C_TEXT_DIM, false);
    rt_status(cv, 230, 10, &v->status, true, now_ms);
    if (v->item == NULL) {
        rt_icon_phone(cv, 104, 120, 2, RT_C_TEXT_DIM);
        (void)rt_text_centered(cv, 120, 170, "NO NOTIFICATIONS", 1, RT_C_TEXT_DIM, false);
        return;
    }
    draw_card(cv, t, v);
    draw_footer(cv, v);
}

void rt_draw_notify_toast(RtCanvas *cv, const RtTheme *t, const RnNotify *n, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL) || n == NULL) {
        return;
    }
    (void)now_ms;
    RtTheme dialog = *t;
    dialog.panel = (uint8_t)RT_PANEL_DOUBLE;
    rt_panel(cv, TOAST_X, TOAST_Y, TOAST_W, TOAST_H, &dialog, false);
    char buf[RN_NOTIFY_TITLE_MAX + 4u];
    const char *title = (n->title[0] != '\0') ? n->title : n->app;
    (void)rt_text(cv, TOAST_X + 10, TOAST_Y + 9, n->app, 1, RT_C_ACCENT, false);
    int32_t app_w = rt_text_width(n->app, 1);
    (void)rt_text(cv, TOAST_X + 16 + app_w, TOAST_Y + 9,
                  rt_fit(title, 1, TOAST_W - 50 - app_w, buf, sizeof buf), 1, RT_C_TEXT, false);
    (void)rt_text(cv, TOAST_X + TOAST_W - 10 - rt_text_width("C", 1), TOAST_Y + 9, "C", 1, RT_C_ACCENT, false);
    char line[RN_NOTIFY_TEXT_MAX + 1u];                         /* one line, ".." when cut */
    (void)rt_text(cv, TOAST_X + 10, TOAST_Y + 31, rt_fit(n->text, 1, TOAST_W - 30, line, sizeof line), 1,
                  RT_C_TEXT_DIM, false);
}
