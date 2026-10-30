/*
 * rt_navview.c - navigation view.
 */
#include "rt_navview.h"

#define RT_GUIDE_COUNTDOWN_S 15.0f      /* longer without news: trust Google's number */

#include <math.h>
#include <string.h>

#include "rn/rn_maneuver.h"
#include "rn/rn_protocol.h"
#include "rn/rn_types.h"
#include "rt_mapview.h"
#include "rt_screens.h"

typedef struct {
    RnLookahead la;
    RnManeuver next;
    RnManeuver then;
    RnManeuver current;
} Upcoming;

static Upcoming upcoming(const RnPack *pack, float along)
{
    Upcoming u;
    (void)memset(&u, 0, sizeof u);
    float alongs[RN_MAX_MANEUVERS];
    uint8_t types[RN_MAX_MANEUVERS];
    uint32_t n = (pack->h.n_maneuvers < RN_MAX_MANEUVERS) ? pack->h.n_maneuvers : RN_MAX_MANEUVERS;
    for (uint32_t i = 0u; i < n; i++) {
        alongs[i] = rn_pack_maneuver_along(pack, i);
        types[i] = rn_pack_maneuver_type(pack, i);
    }
    u.la = rn_maneuver_lookahead(alongs, types, n, along);
    if (u.la.next >= 0) {
        (void)rn_pack_maneuver(pack, (uint32_t)u.la.next, &u.next);
    }
    if (u.la.then >= 0) {
        (void)rn_pack_maneuver(pack, (uint32_t)u.la.then, &u.then);
    }
    if (u.la.current >= 0) {
        (void)rn_pack_maneuver(pack, (uint32_t)u.la.current, &u.current);
    }
    return u;
}

static RtHudData hud_data(const RnPack *pack, const RnTracker *trk, const Upcoming *u, const RtNavViewOpts *opt)
{
    RtHudData d;
    (void)memset(&d, 0, sizeof d);
    float along = trk->along_m;
    d.next = (u->la.next >= 0) ? u->next.type : (uint8_t)RN_M_NONE;
    d.next_dist_m = (u->la.next >= 0) ? u->next.along_m - along : 0.0f;
    d.next_road = (u->la.next >= 0) ? u->next.road : "";
    d.flyover = (u->la.next >= 0) ? u->next.hint : (uint8_t)RN_FLY_NONE;
    d.then = (u->la.then >= 0) ? u->then.type : (uint8_t)RN_M_NONE;
    d.current_road = (u->la.current >= 0) ? u->current.road : "";
    d.speed_kmh = (int32_t)lroundf(trk->speed_mps * 3.6f);
    d.eta_s = trk->last.eta_s;
    d.remaining_m = fmaxf(pack->h.total_m - along, 0.0f);
    d.off_route = (trk->last.flags & RN_NAV_OFF_ROUTE) != 0u;
    d.link_lost = opt->link_lost;
    d.gps_stale = opt->gps_stale;
    d.have_clock = opt->have_clock;
    d.time_s = opt->time_s;
    d.busy = opt->busy;
    d.status = opt->status;
    return d;
}

/* Free mode: the HUD shows Google Maps' turn (GUIDANCE), or just speed. */
static RtHudData hud_free(const RnTracker *trk, const RtNavViewOpts *opt)
{
    RtHudData d;
    (void)memset(&d, 0, sizeof d);
    d.next = (uint8_t)RN_M_NONE;
    d.then = (uint8_t)RN_M_NONE;
    d.next_road = "START GOOGLE MAPS";
    d.current_road = "";
    d.flyover = (uint8_t)RN_FLY_NONE;
    d.speed_kmh = (int32_t)lroundf(trk->speed_mps * 3.6f);
    d.eta_s = trk->last.eta_s;
    d.remaining_m = trk->last.remaining_m;
    const RnGuidance *g = opt->guide;
    if (g != NULL && g->maneuver < (uint8_t)RN_M_COUNT) {
        bool arrived = (g->flags & RN_GUIDE_ARRIVED) != 0u;
        d.next = arrived ? (uint8_t)RN_M_ARRIVE : g->maneuver;
        /* Google updates the distance every few seconds: count it down with
         * our own speed in between, so the number moves smoothly every frame. */
        float age_s = fminf((float)opt->guide_age_ms * 0.001f, RT_GUIDE_COUNTDOWN_S);
        float said = (g->to_turn_m == RN_GUIDE_UNKNOWN16) ? 0.0f : (float)g->to_turn_m;
        d.next_dist_m = (said > 0.0f) ? fmaxf(said - trk->speed_mps * age_s, 0.0f) : 0.0f;
        d.next_road = g->road;
        d.eta_s = (g->eta_s == RN_GUIDE_UNKNOWN32) ? trk->last.eta_s : g->eta_s;
        d.remaining_m = (g->remaining_m == RN_GUIDE_UNKNOWN32) ? trk->last.remaining_m : (float)g->remaining_m;
    }
    d.link_lost = opt->link_lost;
    d.gps_stale = opt->gps_stale;
    d.no_line = opt->guide != NULL && (trk->last.flags & RN_NAV_LINE) == 0u;
    d.have_clock = opt->have_clock;
    d.time_s = opt->time_s;
    d.busy = opt->busy;
    d.status = opt->status;
    return d;
}

static void draw_free(RtCanvas *cv, const RtTheme *t, const RnPack *pack, RnTileCache *cache,
                      const RnTracker *trk, const RtNavViewOpts *opt, uint32_t now_ms)
{
    RtCamera cam = {trk->x_px, trk->y_px, opt->north_up ? 0.0f : trk->heading_rad, RT_ANCHOR_X,
                    opt->north_up ? RT_ANCHOR_Y_NORTH : RT_ANCHOR_Y};
    rt_map_draw(cv, pack, cache, &cam);
    if ((trk->last.flags & RN_NAV_LINE) != 0u) {         /* Google's route as the blue line */
        RnLine pack_line = rn_line_from_pack(pack);
        rt_map_line(cv, (opt->line != NULL) ? opt->line : &pack_line, &cam, trk->along_m, -1.0f);
    }
    if (!opt->overlays) {
        return;
    }
    rt_map_rider(cv, &cam, trk->heading_rad);
    RtHudData d = hud_free(trk, opt);
    rt_draw_hud(cv, t, &d, now_ms);
}

void rt_navview_draw(RtCanvas *cv, const RtTheme *t, const RnPack *pack, RnTileCache *cache,
                     const RnTracker *trk, const RtNavViewOpts *opt, uint32_t now_ms)
{
    if (!RN_ASSERT(cv != NULL && t != NULL && pack != NULL && cache != NULL) ||
        !RN_ASSERT(trk != NULL && opt != NULL)) {
        return;
    }
    if (trk->free) {
        draw_free(cv, t, pack, cache, trk, opt, now_ms);
        return;
    }
    float along = trk->along_m;
    Upcoming u = upcoming(pack, along);
    RnPackPoint here = rn_pack_position_at(pack, along);
    /* Rider sits low on the screen so most of the view is the road ahead. */
    RtCamera cam = {here.x, here.y, opt->north_up ? 0.0f : trk->heading_rad, RT_ANCHOR_X,
                    opt->north_up ? RT_ANCHOR_Y_NORTH : RT_ANCHOR_Y};
    rt_map_draw(cv, pack, cache, &cam);
    rt_map_route(cv, pack, &cam, along, (u.la.next >= 0) ? u.next.along_m : -1.0f);
    if (!opt->overlays) {
        return;
    }
    rt_map_rider(cv, &cam, trk->heading_rad);
    RtHudData d = hud_data(pack, trk, &u, opt);
    rt_draw_hud(cv, t, &d, now_ms);
}
