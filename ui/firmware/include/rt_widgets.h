/*
 * rt_widgets.h - reusable UI pieces: floating panels (Hyprland window or
 * Pokemon dialog box depending on the theme), chips, the phone status
 * indicator, and text fitting.
 */
#ifndef RT_WIDGETS_H
#define RT_WIDGETS_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_canvas.h"
#include "rt_theme.h"

/* Phone battery + link, shown on every screen. */
typedef struct {
    bool known;        /* ever heard from the phone   */
    bool link_ok;      /* packets arriving            */
    uint8_t battery;   /* %                           */
    bool charging;
    uint8_t bars;      /* mobile signal 0..4          */
    bool google_cap;   /* phone near its Google limit: "G!" */
} RtStatusView;

/* glass = the map stays visible through the panel, dimmed. */
void rt_panel(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h, const RtTheme *t, bool glass);
/* Small solid panel with one line of text, centred on cx. */
void rt_chip(RtCanvas *cv, const RtTheme *t, int32_t cx, int32_t y, const char *text, uint8_t color);
/* Battery + signal right-aligned at right_x; with_pct adds "78%". */
void rt_status(RtCanvas *cv, int32_t right_x, int32_t y, const RtStatusView *s, bool with_pct, uint32_t now_ms);
/* Copy s into out (cap bytes), cut with ".." to fit max_w pixels. */
const char *rt_fit(const char *s, int32_t scale, int32_t max_w, char *out, uint32_t cap);
/* Sparse dot grid background (the "void" of a Game Boy map). */
void rt_dot_grid(RtCanvas *cv, int32_t phase);

#endif /* RT_WIDGETS_H */
