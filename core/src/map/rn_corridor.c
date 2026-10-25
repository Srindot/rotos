/*
 * rn_corridor.c - route corridor planning.
 */
#include "rn/rn_corridor.h"

#define RN_CORRIDOR_BLOCK_CELLS 8u    /* = MapPackBuilder.BLOCK_CELLS */

#include <math.h>
#include <string.h>

#include "rn/rn_rle.h"

typedef struct {
    double x0;
    double y0;
    double x1;
    double y1;
} Bounds;

static Bounds route_bounds(const RnRoute *r, uint8_t zoom)
{
    Bounds b = {1e18, 1e18, -1e18, -1e18};
    for (uint32_t i = 0u; i < r->n_pts && i < RN_MAX_ROUTE_PTS; i++) {
        double x = 0.0;
        double y = 0.0;
        rn_geo_world_px(r->pts[i].lat, r->pts[i].lon, zoom, &x, &y);
        b.x0 = fmin(b.x0, x);
        b.y0 = fmin(b.y0, y);
        b.x1 = fmax(b.x1, x);
        b.y1 = fmax(b.y1, y);
    }
    return b;
}

static void set_keep(RnCorridor *c, uint32_t cx, uint32_t cy)
{
    uint32_t cell = cy * c->tiles_w + cx;
    uint8_t bit = (uint8_t)(1u << (cell & 7u));
    if ((c->keep[cell >> 3] & bit) == 0u) {
        c->keep[cell >> 3] |= bit;
        c->n_keep++;
    }
}

static float seg_dist(RnVec2 p, RnVec2 a, RnVec2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float l2 = dx * dx + dy * dy;
    float t = (l2 > 1e-6f) ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0.0f;
    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    float ex = a.x + dx * t - p.x;
    float ey = a.y + dy * t - p.y;
    return sqrtf(ex * ex + ey * ey);
}

/* Mark every cell whose centre is within `reach` px of segment a-b. */
static void mark_segment(RnCorridor *c, RnVec2 a, RnVec2 b, float reach)
{
    float t = (float)RN_TILE;
    int32_t cx0 = (int32_t)floorf((fminf(a.x, b.x) - reach) / t);
    int32_t cy0 = (int32_t)floorf((fminf(a.y, b.y) - reach) / t);
    int32_t cx1 = (int32_t)floorf((fmaxf(a.x, b.x) + reach) / t);
    int32_t cy1 = (int32_t)floorf((fmaxf(a.y, b.y) + reach) / t);
    cx0 = (cx0 < 0) ? 0 : cx0;
    cy0 = (cy0 < 0) ? 0 : cy0;
    cx1 = (cx1 >= (int32_t)c->tiles_w) ? (int32_t)c->tiles_w - 1 : cx1;
    cy1 = (cy1 >= (int32_t)c->tiles_h) ? (int32_t)c->tiles_h - 1 : cy1;
    for (int32_t cy = cy0; cy <= cy1; cy++) {
        for (int32_t cx = cx0; cx <= cx1; cx++) {
            RnVec2 centre = {((float)cx + 0.5f) * t, ((float)cy + 0.5f) * t};
            if (seg_dist(centre, a, b) < reach) {
                set_keep(c, (uint32_t)cx, (uint32_t)cy);
            }
        }
    }
}

RnVec2 rn_corridor_point_px(const RnCorridor *c, const RnRoute *r, uint32_t i)
{
    RnVec2 p = {0.0f, 0.0f};
    if (!RN_ASSERT(c != NULL && r != NULL) || !RN_ASSERT(i < r->n_pts)) {
        return p;
    }
    double x = 0.0;
    double y = 0.0;
    rn_geo_world_px(r->pts[i].lat, r->pts[i].lon, c->zoom, &x, &y);
    p.x = (float)(x - (double)c->origin_x);
    p.y = (float)(y - (double)c->origin_y);
    return p;
}

rn_status rn_corridor_plan(RnCorridor *c, const RnRoute *r, uint8_t zoom, uint32_t buffer_px)
{
    if (!RN_ASSERT(c != NULL && r != NULL) || !RN_ASSERT(zoom >= 10u && zoom <= 20u)) {
        return RN_ERR_PARAM;
    }
    if (r->n_pts < 2u) {
        return RN_ERR_STATE;
    }
    (void)memset(c, 0, sizeof *c);
    Bounds b = route_bounds(r, zoom);
    double t = (double)RN_TILE;
    double buf = (double)buffer_px;
    c->zoom = zoom;
    c->buffer_px = buffer_px;
    /* Origin on the global 256 px block grid (8 x 8 cells, what rn_mapgen
     * classifies at a time): the same street then gives byte-identical tiles
     * in every map, so a new map can be sent as a patch of the old one. */
    double align = t * (double)RN_CORRIDOR_BLOCK_CELLS;
    c->origin_x = (int32_t)(floor((b.x0 - buf) / align) * align);
    c->origin_y = (int32_t)(floor((b.y0 - buf) / align) * align);
    double w = ceil((b.x1 + buf - (double)c->origin_x) / t);
    double h = ceil((b.y1 + buf - (double)c->origin_y) / t);
    if (w < 1.0 || h < 1.0 || w > (double)RN_CORRIDOR_MAX_SIDE || h > (double)RN_CORRIDOR_MAX_SIDE) {
        return RN_ERR_RANGE;
    }
    c->tiles_w = (uint16_t)w;
    c->tiles_h = (uint16_t)h;
    c->m_per_px = (float)rn_geo_m_per_px(r->pts[r->n_pts / 2u].lat, zoom);
    float reach = (float)buffer_px + (float)RN_TILE * 0.71f;
    RnVec2 prev = rn_corridor_point_px(c, r, 0u);
    for (uint32_t i = 1u; i < r->n_pts && i < RN_MAX_ROUTE_PTS; i++) {
        RnVec2 cur = rn_corridor_point_px(c, r, i);
        mark_segment(c, prev, cur, reach);
        prev = cur;
    }
    return (c->n_keep <= 65534u) ? RN_OK : RN_ERR_RANGE;
}

bool rn_corridor_keep(const RnCorridor *c, uint32_t cx, uint32_t cy)
{
    if (!RN_ASSERT(c != NULL) || cx >= c->tiles_w || cy >= c->tiles_h) {
        return false;
    }
    uint32_t cell = cy * c->tiles_w + cx;
    return (c->keep[cell >> 3] & (uint8_t)(1u << (cell & 7u))) != 0u;
}
