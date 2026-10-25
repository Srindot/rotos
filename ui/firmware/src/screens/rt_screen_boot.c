/*
 * rt_screen_boot.c - boot logo and the progress panels (phone preparing a
 * route, map transfer). The no-phone idle screen is rt_screen_idle.c.
 */
#include <math.h>

#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_screens.h"

static float clamp01(float x)
{
    return (x < 0.0f) ? 0.0f : ((x > 1.0f) ? 1.0f : x);
}

void rt_draw_boot(RtCanvas *cv, const RtTheme *t, uint32_t ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL)) {
        return;
    }
    rt_canvas_clear(cv, RT_C_LAND);
    int32_t off = (int32_t)((ms / 40u) % 8u);                /* slow scrolling dots */
    for (int32_t y = off; y < RT_SCREEN_H; y += 8) {
        for (int32_t x = 4; x < RT_SCREEN_W; x += 8) {
            rt_canvas_pixel(cv, x, y, RT_C_BUILDING);
        }
    }
    /* Game Boy style: the logo slides down and stops dead in the centre. */
    float p = clamp01((float)ms / 900.0f);
    int32_t y = (int32_t)(-50.0f + 168.0f * p);
    (void)rt_text_centered(cv, 120, y, "ROTOS", 6, RT_C_ACCENT, true);
    if (ms > 1000u) {
        (void)rt_text_centered(cv, 120, 172, "MOTORCYCLE SYSTEM", 1, RT_C_TEXT, false);
        int32_t segs = (int32_t)(clamp01((float)(ms - 1000u) / 800.0f) * 12.0f);
        rt_panel(cv, 58, 196, 124, 14, t, false);
        for (int32_t i = 0; i < segs; i++) {
            rt_canvas_fill_rect(cv, 62 + i * 10, 200, 8, 6, ((i % 2) != 0) ? RT_C_HILITE : RT_C_ACCENT);
        }
    }
    (void)rt_text_centered(cv, 120, 300, RT_VERSION_TEXT, 1, RT_C_TEXT_DIM, false);
}

/* Footer panel: blinking title, value on the right, chunky progress bar. */
static void progress_panel(RtCanvas *cv, const RtTheme *t, const char *title, const char *value, uint32_t pct,
                           uint32_t now_ms)
{
    pct = (pct > 100u) ? 100u : pct;
    rt_panel(cv, 12, 268, 216, 46, t, false);                /* over the footer */
    bool blink = ((now_ms / 400u) & 1u) != 0u;
    (void)rt_text(cv, 22, 277, title, 1, blink ? RT_C_TEXT : RT_C_ACCENT, false);
    (void)rt_text(cv, 218 - rt_text_width(value, 1), 277, value, 1, RT_C_TEXT_DIM, false);
    rt_canvas_rect(cv, 22, 291, 196, 12, RT_C_ACCENT);
    int32_t segs = (int32_t)(pct * 19u / 100u);               /* chunky segments */
    for (int32_t i = 0; i < segs; i++) {
        rt_canvas_fill_rect(cv, 25 + i * 10, 294, 8, 6, ((i % 2) != 0) ? RT_C_HILITE : RT_C_ACCENT);
    }
}

void rt_draw_map_progress(RtCanvas *cv, const RtTheme *t, uint32_t received, uint32_t total, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL)) {
        return;
    }
    uint32_t pct = (total > 0u) ? (uint32_t)((uint64_t)received * 100u / total) : 0u;
    char v[24] = "";
    char tot[12] = "";
    (void)rt_fmt_u32(v, sizeof v, received / 1024u, 1u);
    (void)rt_fmt_cat(v, sizeof v, " / ");
    (void)rt_fmt_u32(tot, sizeof tot, total / 1024u, 1u);
    (void)rt_fmt_cat(v, sizeof v, tot);
    (void)rt_fmt_cat(v, sizeof v, " KB");
    progress_panel(cv, t, "LOADING MAP", v, pct, now_ms);
}

void rt_draw_prep_progress(RtCanvas *cv, const RtTheme *t, uint8_t stage, uint8_t pct, uint16_t done,
                           uint16_t total, uint32_t now_ms)
{
    static const char *const TITLE[RN_PREP_STAGE_MAX + 1u] = {
        "", "PHONE: FINDING ROUTE", "PHONE: FLYOVERS", "PHONE: BUILDING MAP", "PHONE: MAP READY",
        "PHONE: ROUTE FAILED"};
    if (!RN_ASSERT(cv != NULL && t != NULL) || !RN_ASSERT(stage <= RN_PREP_STAGE_MAX)) {
        return;
    }
    char v[16] = "";
    if (total > 0u) {
        char tot[8] = "";
        (void)rt_fmt_u32(v, sizeof v, done, 1u);
        (void)rt_fmt_cat(v, sizeof v, "/");
        (void)rt_fmt_u32(tot, sizeof tot, total, 1u);
        (void)rt_fmt_cat(v, sizeof v, tot);
    } else {
        (void)rt_fmt_u32(v, sizeof v, pct, 1u);
        (void)rt_fmt_cat(v, sizeof v, "%");
    }
    progress_panel(cv, t, TITLE[stage], v, pct, now_ms);
}
