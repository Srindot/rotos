/*
 * rt_screen_call.c - incoming call bar, in the Pokemon dialog style
 * (double border) so it reads as "something is asking you". It sits at the
 * bottom over the speed panel: the road ahead and the next turn stay clear.
 */
#include "rn/rn_types.h"
#include "rt_call.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_icons.h"
#include "rt_screens.h"

#define BOX_X 6
#define BOX_Y 262
#define BOX_W 228
#define BOX_H 52

/* Game Boy style round button with its letter, inverted on the bright bar. */
static void button_badge(RtCanvas *cv, int32_t x, int32_t y, const char *letter, uint8_t bg)
{
    rt_canvas_fill_circle(cv, x, y, 7, RT_C_PANEL);
    (void)rt_text_centered(cv, x + 1, y - 3, letter, 1, bg, false);
}

/* Row 1: ringing phone, "CALL" and who is calling. */
static void draw_header(RtCanvas *cv, const RtCallView *c, uint32_t now_ms)
{
    bool ringing = c->ui == (uint8_t)RT_CALL_RINGING;
    int32_t shake = (ringing && ((now_ms / 120u) & 1u) != 0u) ? 1 : 0;
    rt_icon_phone(cv, BOX_X + 8 + shake, BOX_Y + 7, 1, RT_C_PANEL);
    (void)rt_text(cv, BOX_X + 30, BOX_Y + 9, "CALL", 1, RT_C_PANEL, false);
    char buf[40];
    const char *name = (c->name != NULL && c->name[0] != '\0') ? c->name : "UNKNOWN";
    (void)rt_text(cv, BOX_X + 62, BOX_Y + 9, rt_fit(name, 1, BOX_W - 70, buf, sizeof buf), 1, RT_C_PANEL, false);
}

/* Row 2: A = reject and text back, B = ignore (and silence the ringtone). */
static void draw_choices(RtCanvas *cv, uint8_t bg)
{
    button_badge(cv, BOX_X + 14, BOX_Y + 36, "A", bg);
    (void)rt_text(cv, BOX_X + 26, BOX_Y + 33, "REPLY RIDING", 1, RT_C_PANEL, false);
    button_badge(cv, BOX_X + 128, BOX_Y + 36, "B", bg);
    (void)rt_text(cv, BOX_X + 140, BOX_Y + 33, "IGNORE", 1, RT_C_PANEL, false);
}

/* Solid bright bar with a dark frame; while ringing the frame flashes. */
static void draw_bar(RtCanvas *cv, uint8_t bg, bool ringing, uint32_t now_ms)
{
    rt_canvas_fill_rect(cv, BOX_X, BOX_Y, BOX_W, BOX_H, bg);
    bool flash = ringing && ((now_ms / 400u) & 1u) != 0u;
    uint8_t edge = flash ? RT_C_PANEL : RT_C_SHADOW;
    for (int32_t k = 0; k < 3; k++) {
        rt_canvas_rect(cv, BOX_X - k, BOX_Y - k, BOX_W + 2 * k, BOX_H + 2 * k, (k == 2) ? RT_C_SHADOW : edge);
    }
}

/* A slim bright bar over the speed panel at the bottom: easy to spot, and the
 * map and the next turn stay visible. Red (day) / green (night) while ringing. */
void rt_draw_call(RtCanvas *cv, const RtTheme *t, const RtCallView *c, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && c != NULL) || c->ui == (uint8_t)RT_CALL_HIDDEN) {
        return;
    }
    bool ringing = c->ui == (uint8_t)RT_CALL_RINGING;
    uint8_t bg = ringing ? (uint8_t)RT_C_HILITE : (uint8_t)RT_C_ACCENT;
    draw_bar(cv, bg, ringing, now_ms);
    draw_header(cv, c, now_ms);
    if (ringing) {
        draw_choices(cv, bg);
    } else if (c->ui == (uint8_t)RT_CALL_SENDING) {
        char s[24] = "SENDING REPLY";
        uint32_t dots = (now_ms / 300u) % 4u;
        for (uint32_t i = 0u; i < dots; i++) {
            (void)rt_fmt_cat(s, sizeof s, ".");
        }
        (void)rt_text(cv, BOX_X + 30, BOX_Y + 33, s, 1, RT_C_PANEL, false);
    } else {
        (void)rt_text(cv, BOX_X + 30, BOX_Y + 33, "REPLY SENT - CALL ENDED", 1, RT_C_PANEL, false);
    }
}
