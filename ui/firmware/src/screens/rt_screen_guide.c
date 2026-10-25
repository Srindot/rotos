/*
 * rt_screen_guide.c - "Google Maps" mode: turn-by-turn from another
 * navigation app (GUIDANCE packets), drawn without a map: a big turn arrow,
 * the distance to the turn, the road, then arrival time and distance left.
 */
#include "rn/rn_maneuver.h"
#include "rn/rn_types.h"
#include "rt_font.h"
#include "rt_format.h"
#include "rt_icons.h"
#include "rt_screens.h"

#define ARROW_SCALE 6
#define ROAD_CHARS  18u

static void draw_turn(RtCanvas *cv, const RtTheme *t, const RtGuideView *g)
{
    rt_panel(cv, 12, 26, 216, 186, t, false);
    uint8_t m = g->arrived ? (uint8_t)RN_M_ARRIVE : g->maneuver;
    rt_icon_maneuver(cv, 120 - 8 * ARROW_SCALE, 34, m, ARROW_SCALE, RT_C_ACCENT);
    if (g->arrived) {
        (void)rt_text_centered(cv, 120, 150, "ARRIVED", 4, RT_C_TEXT, false);
        return;
    }
    if (g->to_turn_m == RN_GUIDE_UNKNOWN16) {
        (void)rt_text_centered(cv, 120, 150, rn_maneuver_name(m), 3, RT_C_TEXT, false);
        return;
    }
    char value[RT_VALUE_LEN];
    char unit[RT_UNIT_LEN];
    rt_fmt_distance((float)g->to_turn_m, value, unit);
    if (g->to_turn_m < 30u) {
        (void)rt_text_centered(cv, 120, 146, "NOW", 6, RT_C_HILITE, false);
        return;
    }
    int32_t wv = rt_text_width(value, 6);
    int32_t wu = rt_text_width(unit, 3);
    int32_t x = 120 - (wv + 8 + wu) / 2;
    (void)rt_text(cv, x, 146, value, 6, RT_C_TEXT, false);
    (void)rt_text(cv, x + wv + 8, 167, unit, 3, RT_C_TEXT_DIM, false);
}

static void draw_road(RtCanvas *cv, const RtGuideView *g)
{
    char road[ROAD_CHARS + 1u];
    uint32_t i = 0u;
    for (; i < ROAD_CHARS && g->road[i] != '\0'; i++) {
        road[i] = g->road[i];
    }
    road[i] = '\0';
    if (i == 0u) {
        return;
    }
    (void)rt_text_centered(cv, 120, 222, road, 2, RT_C_TEXT, false);
}

static void draw_trip(RtCanvas *cv, const RtTheme *t, const RtGuideView *g)
{
    rt_panel(cv, 12, 248, 216, 50, t, false);
    char eta[12] = "--:--";
    if (g->eta_s != RN_GUIDE_UNKNOWN32) {
        rt_fmt_hhmm(g->eta_s, eta, sizeof eta);
    }
    (void)rt_text(cv, 24, 256, "ARRIVE", 1, RT_C_TEXT_DIM, false);
    (void)rt_text(cv, 24, 268, eta, 3, RT_C_TEXT, false);
    char value[RT_VALUE_LEN];
    char unit[RT_UNIT_LEN];
    if (g->remaining_m == RN_GUIDE_UNKNOWN32) {
        value[0] = '-';
        value[1] = '\0';
        unit[0] = '\0';
    } else {
        rt_fmt_distance((float)g->remaining_m, value, unit);
    }
    (void)rt_text(cv, 216 - rt_text_width("LEFT", 1), 256, "LEFT", 1, RT_C_TEXT_DIM, false);
    int32_t wu = rt_text_width(unit, 2);
    int32_t wv = rt_text_width(value, 3);
    (void)rt_text(cv, 216 - wu, 275, unit, 2, RT_C_TEXT_DIM, false);
    (void)rt_text(cv, 216 - wu - 6 - wv, 268, value, 3, RT_C_TEXT, false);
}

void rt_draw_guidance(RtCanvas *cv, const RtTheme *t, const RtGuideView *g, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && g != NULL) || !RN_ASSERT(g->maneuver < (uint8_t)RN_M_COUNT)) {
        return;
    }
    rt_canvas_clear(cv, RT_C_LAND);
    rt_dot_grid(cv, 0);
    (void)rt_text(cv, 10, 8, "GOOGLE MAPS", 1, RT_C_ROUTE, false);
    rt_status(cv, 230, 8, &g->status, false, now_ms);
    draw_turn(cv, t, g);
    draw_road(cv, g);
    draw_trip(cv, t, g);
}
