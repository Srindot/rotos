/*
 * rn_flyover.c - flyover / service road hints from OSM bridge geometry.
 */
#include "rn/rn_flyover.h"

#include <math.h>
#include <string.h>

#include "rn/rn_text.h"

#define ON_BRIDGE_DIST_M     8.0f    /* route within this of a bridge ...     */
#define PARALLEL_DEG        25.0f    /* ... and parallel to it (not crossing) */
#define START_NEAR_M        35.0f
#define START_DIR_DEG       30.0f
#define SYNTH_FIRST_M       50
#define SYNTH_TAIL_M       150
#define SYNTH_STEP_M        10
#define SYNTH_SPACING_M    200.0f
#define SYNTH_CLEAR_M      120.0f

void rn_bridges_clear(RnBridgeSet *b)
{
    if (!RN_ASSERT(b != NULL)) {
        return;
    }
    b->n_bridges = 0u;
    b->n_pts = 0u;
    b->start[0] = 0u;
}

rn_status rn_bridges_add(RnBridgeSet *b, const RnRoute *r, const double *lat,
                         const double *lon, uint32_t n)
{
    if (!RN_ASSERT(b != NULL && r != NULL && lat != NULL && lon != NULL)) {
        return RN_ERR_PARAM;
    }
    if (n < 2u) {
        return RN_ERR_RANGE;
    }
    if (b->n_bridges >= RN_MAX_BRIDGES || b->n_pts + n > RN_MAX_BRIDGE_PTS) {
        return RN_ERR_FULL;
    }
    float len = 0.0f;
    for (uint32_t i = 0u; i < n; i++) {
        RnVec2 p = rn_geo_to_local(&r->frame, lat[i], lon[i]);
        if (i > 0u) {
            RnVec2 q = b->pts[b->n_pts + i - 1u];
            len += hypotf(p.x - q.x, p.y - q.y);
        }
        b->pts[b->n_pts + i] = p;
    }
    if (len < RN_FLY_MIN_BRIDGE_M) {
        return RN_ERR_RANGE;
    }
    b->n_pts += n;
    b->n_bridges++;
    b->start[b->n_bridges] = (uint16_t)b->n_pts;
    return RN_OK;
}

static float angle_deg(RnVec2 u, RnVec2 v)
{
    float nu = hypotf(u.x, u.y);
    float nv = hypotf(v.x, v.y);
    if (nu < 1e-6f || nv < 1e-6f) {
        return 180.0f;
    }
    float c = (u.x * v.x + u.y * v.y) / (nu * nv);
    c = (c > 1.0f) ? 1.0f : ((c < -1.0f) ? -1.0f : c);
    return acosf(c) * (float)RN_RAD2DEG;
}

static float seg_dist(RnVec2 p, RnVec2 a, RnVec2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / fmaxf(dx * dx + dy * dy, 1e-9f);
    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    return hypotf(p.x - a.x - dx * t, p.y - a.y - dy * t);
}

static RnVec2 heading_vec(const RnRoute *r, float s)
{
    RnVec2 a = rn_route_position_at(r, s - 5.0f);
    RnVec2 b = rn_route_position_at(r, s + 5.0f);
    RnVec2 h = {b.x - a.x, b.y - a.y};
    return h;
}

static bool on_bridge(const RnRoute *r, const RnBridgeSet *b, float s)
{
    RnVec2 p = rn_route_position_at(r, s);
    RnVec2 h = heading_vec(r, s);
    for (uint32_t k = 0u; k < b->n_bridges; k++) {
        for (uint32_t i = b->start[k]; i + 1u < b->start[k + 1u]; i++) {
            RnVec2 v = {b->pts[i + 1u].x - b->pts[i].x, b->pts[i + 1u].y - b->pts[i].y};
            RnVec2 nv = {-v.x, -v.y};
            if (seg_dist(p, b->pts[i], b->pts[i + 1u]) < ON_BRIDGE_DIST_M &&
                fminf(angle_deg(h, v), angle_deg(h, nv)) < PARALLEL_DEG) {
                return true;
            }
        }
    }
    return false;
}

static bool end_points_our_way(RnVec2 p, RnVec2 h, RnVec2 end, RnVec2 next)
{
    RnVec2 dir = {next.x - end.x, next.y - end.y};
    return hypotf(p.x - end.x, p.y - end.y) < START_NEAR_M && angle_deg(h, dir) < START_DIR_DEG;
}

static bool start_near(const RnRoute *r, const RnBridgeSet *b, float s)
{
    RnVec2 p = rn_route_position_at(r, s);
    RnVec2 h = heading_vec(r, s);
    for (uint32_t k = 0u; k < b->n_bridges; k++) {
        uint32_t first = b->start[k];
        uint32_t last = (uint32_t)b->start[k + 1u] - 1u;
        if (end_points_our_way(p, h, b->pts[first], b->pts[first + 1u]) ||
            end_points_our_way(p, h, b->pts[last], b->pts[last - 1u])) {
            return true;
        }
    }
    return false;
}

/* Sample index range [i0, i1) for metres [a, b). */
static void sample_range(const RnFlyoverWork *w, float a, float b, uint32_t *i0, uint32_t *i1)
{
    float fa = floorf(a / RN_FLY_STEP_M);
    float fb = floorf(b / RN_FLY_STEP_M);
    *i0 = (fa < 0.0f) ? 0u : (uint32_t)fa;
    *i1 = (fb < 0.0f) ? 0u : (((uint32_t)fb > w->n) ? w->n : (uint32_t)fb);
}

static float frac_on(const RnFlyoverWork *w, float a, float b)
{
    uint32_t i0 = 0u;
    uint32_t i1 = 0u;
    sample_range(w, a, b, &i0, &i1);
    if (i1 <= i0) {
        return 0.0f;
    }
    uint32_t on = 0u;
    for (uint32_t i = i0; i < i1; i++) {
        on += w->on_bridge[i];
    }
    return (float)on / (float)(i1 - i0);
}

static bool starts_in(const RnFlyoverWork *w, float a, float b)
{
    uint32_t i0 = 0u;
    uint32_t i1 = 0u;
    sample_range(w, a, b, &i0, &i1);
    for (uint32_t i = i0; i < i1; i++) {
        if (w->start_near[i] != 0u) {
            return true;
        }
    }
    return false;
}

static uint8_t decide(const RnFlyoverWork *w, float a)
{
    bool before = frac_on(w, a - 60.0f, a - 10.0f) > 0.5f;
    bool after = frac_on(w, a + 20.0f, a + 150.0f) > 0.5f;
    if (after && !before) {
        return (uint8_t)RN_FLY_TAKE;
    }
    if (before && !after) {
        return (uint8_t)RN_FLY_EXIT;
    }
    if (!before && !after && starts_in(w, a - 10.0f, a + 60.0f)) {
        return (uint8_t)RN_FLY_STAY_BELOW;
    }
    return (uint8_t)RN_FLY_NONE;
}

static bool is_decision_free(uint8_t type)
{
    return type == (uint8_t)RN_M_DEPART || type == (uint8_t)RN_M_NAME_CHANGE ||
           type == (uint8_t)RN_M_ARRIVE;
}

static uint8_t synthetic_hint(const RnFlyoverWork *w, float s)
{
    if (frac_on(w, s - 40.0f, s - 5.0f) < 0.3f && frac_on(w, s + 10.0f, s + 120.0f) > 0.7f) {
        return (uint8_t)RN_FLY_TAKE;
    }
    if (frac_on(w, s - 40.0f, s + 120.0f) < 0.2f && starts_in(w, s, s + 10.0f)) {
        return (uint8_t)RN_FLY_STAY_BELOW;
    }
    return (uint8_t)RN_FLY_NONE;
}

static bool near_announced(const RnRoute *r, uint32_t n_orig, float s)
{
    for (uint32_t i = 0u; i < n_orig; i++) {
        if (!is_decision_free(r->man[i].type) && fabsf(s - r->man[i].along_m) < SYNTH_CLEAR_M) {
            return true;
        }
    }
    return false;
}

static void road_before(const RnRoute *r, float s, char *out, uint32_t cap)
{
    out[0] = '\0';
    for (uint32_t i = 0u; i < r->n_man; i++) {
        if (r->man[i].along_m <= s) {
            (void)rn_text_copy(out, cap, r->man[i].road);
        }
    }
}

/* Decision points Google does not announce at all. Returns hints added. */
static int32_t add_synthetic(RnRoute *r, const RnFlyoverWork *w)
{
    uint32_t n_orig = r->n_man;
    float last = -1e9f;
    int32_t added = 0;
    int32_t end = (int32_t)r->total_m - SYNTH_TAIL_M;
    for (int32_t si = SYNTH_FIRST_M; si < end; si += SYNTH_STEP_M) {
        float s = (float)si;
        if (s - last < SYNTH_SPACING_M || near_announced(r, n_orig, s)) {
            continue;
        }
        uint8_t h = synthetic_hint(w, s);
        if (h == (uint8_t)RN_FLY_NONE) {
            continue;
        }
        RnManeuver m;
        (void)memset(&m, 0, sizeof m);
        m.type = (uint8_t)RN_M_STRAIGHT;
        m.hint = h;
        m.along_m = s;
        m.point_idx = (uint16_t)rn_route_segment_at(r, s);
        road_before(r, s, m.road, (uint32_t)sizeof m.road);
        if (rn_route_insert_maneuver(r, &m) != RN_OK) {
            break;
        }
        n_orig++;                                 /* list shifted; keep scanning all */
        added++;
        last = s;
    }
    return added;
}

int32_t rn_flyover_annotate(RnRoute *r, const RnBridgeSet *b, RnFlyoverWork *w)
{
    if (!RN_ASSERT(r != NULL && b != NULL && w != NULL) || !RN_ASSERT(r->total_m <= RN_MAX_ROUTE_M)) {
        return RN_ERR_PARAM;
    }
    w->n = (uint32_t)(r->total_m / RN_FLY_STEP_M) + 1u;
    for (uint32_t i = 0u; i < w->n && i < RN_FLY_MAX_SAMPLES; i++) {
        float s = (float)i * RN_FLY_STEP_M;
        w->on_bridge[i] = on_bridge(r, b, s) ? 1u : 0u;
        w->start_near[i] = start_near(r, b, s) ? 1u : 0u;
    }
    int32_t hints = 0;
    for (uint32_t i = 0u; i < r->n_man; i++) {
        RnManeuver *m = &r->man[i];
        m->hint = is_decision_free(m->type) ? (uint8_t)RN_FLY_NONE : decide(w, m->along_m);
        hints += (m->hint != (uint8_t)RN_FLY_NONE) ? 1 : 0;
    }
    return hints + add_synthetic(r, w);
}

int32_t rn_flyover_by_name(RnRoute *r)
{
    if (!RN_ASSERT(r != NULL) || !RN_ASSERT(r->n_man <= RN_MAX_MANEUVERS)) {
        return RN_ERR_PARAM;
    }
    int32_t hints = 0;
    bool was = false;
    for (uint32_t i = 0u; i < r->n_man; i++) {
        RnManeuver *m = &r->man[i];
        bool here = rn_text_contains_ci(m->road, "FLYOVER");
        m->hint = (uint8_t)RN_FLY_NONE;
        if (!is_decision_free(m->type)) {
            m->hint = (here && !was) ? (uint8_t)RN_FLY_TAKE
                                     : ((was && !here) ? (uint8_t)RN_FLY_EXIT : (uint8_t)RN_FLY_NONE);
        }
        hints += (m->hint != (uint8_t)RN_FLY_NONE) ? 1 : 0;
        was = (m->road[0] != '\0') ? here : was;
    }
    return hints;
}
