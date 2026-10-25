/*
 * rn_route.c - phone-side route model.
 */
#include "rn/rn_route.h"

#include <math.h>
#include <string.h>

#include "rn/rn_text.h"

#define BSEARCH_STEPS 32u   /* > log2(RN_MAX_ROUTE_PTS) */

void rn_route_clear(RnRoute *r)
{
    if (!RN_ASSERT(r != NULL)) {
        return;
    }
    r->n_pts = 0u;
    r->n_man = 0u;
    r->total_m = 0.0f;
    r->duration_s = 0.0f;
    r->route_id = 0u;
    r->frame = rn_geo_frame(0.0, 0.0);
}

rn_status rn_route_add_point(RnRoute *r, double lat, double lon)
{
    if (!RN_ASSERT(r != NULL) || !RN_ASSERT(isfinite(lat) && isfinite(lon)) ||
        !RN_ASSERT(lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0)) {
        return RN_ERR_PARAM;
    }
    if (r->n_pts == 0u) {
        r->frame = rn_geo_frame(lat, lon);
    } else {
        const RnRoutePoint *last = &r->pts[r->n_pts - 1u];
        if (rn_geo_haversine_m(last->lat, last->lon, lat, lon) < RN_ROUTE_MIN_STEP_M) {
            return RN_OK;                      /* junction repeats and dense points add nothing */
        }
    }
    if (r->n_pts >= RN_MAX_ROUTE_PTS) {
        return RN_ERR_FULL;
    }
    RnRoutePoint *p = &r->pts[r->n_pts];
    p->lat = lat;
    p->lon = lon;
    p->xy = rn_geo_to_local(&r->frame, lat, lon);
    p->along_m = 0.0f;
    if (r->n_pts > 0u) {
        const RnRoutePoint *q = &r->pts[r->n_pts - 1u];
        p->along_m = q->along_m + (float)rn_geo_haversine_m(q->lat, q->lon, lat, lon);
    }
    r->n_pts++;
    return RN_OK;
}

rn_status rn_route_add_maneuver(RnRoute *r, uint8_t type, uint32_t point_idx, const char *road)
{
    if (!RN_ASSERT(r != NULL) || !RN_ASSERT(type < (uint8_t)RN_M_COUNT)) {
        return RN_ERR_PARAM;
    }
    if (point_idx >= r->n_pts) {
        return RN_ERR_RANGE;
    }
    uint32_t cap = (type == (uint8_t)RN_M_ARRIVE) ? RN_MAX_MANEUVERS : RN_MAX_MANEUVERS - 1u;
    if (r->n_man >= cap) {                     /* the last slot is kept for ARRIVE */
        return RN_ERR_FULL;
    }
    RnManeuver *m = &r->man[r->n_man];
    (void)memset(m, 0, sizeof *m);
    m->type = type;
    m->hint = (uint8_t)RN_FLY_NONE;
    m->point_idx = (uint16_t)point_idx;
    m->along_m = r->pts[point_idx].along_m;
    (void)rn_text_ascii_upper(m->road, (uint32_t)sizeof m->road, road);
    r->n_man++;
    return RN_OK;
}

rn_status rn_route_finish(RnRoute *r, float duration_s, uint32_t route_id)
{
    if (!RN_ASSERT(r != NULL) || !RN_ASSERT(duration_s >= 0.0f)) {
        return RN_ERR_PARAM;
    }
    if (r->n_pts < 2u) {
        return RN_ERR_STATE;
    }
    r->total_m = r->pts[r->n_pts - 1u].along_m;
    if (!isfinite(r->total_m)) {
        return RN_ERR_FORMAT;
    }
    r->duration_s = duration_s;
    r->route_id = route_id;
    bool has_arrive = r->n_man > 0u && r->man[r->n_man - 1u].type == (uint8_t)RN_M_ARRIVE;
    if (!has_arrive) {
        return rn_route_add_maneuver(r, (uint8_t)RN_M_ARRIVE, r->n_pts - 1u, "");
    }
    return RN_OK;
}

uint32_t rn_route_segment_at(const RnRoute *r, float s)
{
    if (!RN_ASSERT(r != NULL) || !RN_ASSERT(r->n_pts >= 2u)) {
        return 0u;
    }
    uint32_t lo = 0u;
    uint32_t hi = r->n_pts - 1u;
    for (uint32_t k = 0u; k < BSEARCH_STEPS && hi - lo > 1u; k++) {
        uint32_t mid = (lo + hi) / 2u;
        if (r->pts[mid].along_m <= s) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

/* Interpolation parameter of s within segment i. */
static float seg_t(const RnRoute *r, uint32_t i, float s)
{
    float a = r->pts[i].along_m;
    float b = r->pts[i + 1u].along_m;
    float t = (b - a > 1e-3f) ? (s - a) / (b - a) : 0.0f;
    return (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
}

RnVec2 rn_route_position_at(const RnRoute *r, float s)
{
    RnVec2 p = {0.0f, 0.0f};
    if (!RN_ASSERT(r != NULL) || r->n_pts < 2u) {
        return p;
    }
    uint32_t i = rn_route_segment_at(r, s);
    float t = seg_t(r, i, s);
    const RnVec2 a = r->pts[i].xy;
    const RnVec2 b = r->pts[i + 1u].xy;
    p.x = a.x + (b.x - a.x) * t;
    p.y = a.y + (b.y - a.y) * t;
    return p;
}

float rn_route_heading_at(const RnRoute *r, float s)
{
    if (!RN_ASSERT(r != NULL) || r->n_pts < 2u) {
        return 0.0f;
    }
    RnVec2 a = rn_route_position_at(r, s - 5.0f);
    RnVec2 b = rn_route_position_at(r, s + 5.0f);
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    if (dx * dx + dy * dy < 1e-4f) {
        uint32_t i = rn_route_segment_at(r, s);
        dx = r->pts[i + 1u].xy.x - r->pts[i].xy.x;
        dy = r->pts[i + 1u].xy.y - r->pts[i].xy.y;
    }
    return atan2f(dx, dy);                    /* east, north: 0 = north */
}

void rn_route_latlon_at(const RnRoute *r, float s, double *lat, double *lon)
{
    if (!RN_ASSERT(r != NULL && lat != NULL && lon != NULL) || r->n_pts < 2u) {
        return;
    }
    uint32_t i = rn_route_segment_at(r, s);
    double t = (double)seg_t(r, i, s);
    *lat = r->pts[i].lat + (r->pts[i + 1u].lat - r->pts[i].lat) * t;
    *lon = r->pts[i].lon + (r->pts[i + 1u].lon - r->pts[i].lon) * t;
}

rn_status rn_route_insert_maneuver(RnRoute *r, const RnManeuver *m)
{
    if (!RN_ASSERT(r != NULL && m != NULL)) {
        return RN_ERR_PARAM;
    }
    if (r->n_man >= RN_MAX_MANEUVERS) {
        return RN_ERR_FULL;
    }
    uint32_t at = r->n_man;
    while (at > 0u && r->man[at - 1u].along_m > m->along_m) {
        r->man[at] = r->man[at - 1u];
        at--;
    }
    r->man[at] = *m;
    r->n_man++;
    return RN_OK;
}
