/*
 * rt_mapview.c - map raster, route and rider.
 */
#include "rt_mapview.h"

#include <math.h>

#include "rn/rn_types.h"

#define FIX_ONE 65536.0f

typedef struct {
    float rx;   /* screen-right, in pack px */
    float ry;
    float fx;   /* forward (screen-up), in pack px */
    float fy;
} Basis;

static Basis basis(float h)
{
    float s = sinf(h);
    float c = cosf(h);
    Basis b = {c, s, s, -c};
    return b;
}

static void to_screen(const RtCamera *cam, const Basis *b, float wx, float wy, float *sx, float *sy)
{
    float dx = wx - cam->x;
    float dy = wy - cam->y;
    *sx = (float)cam->ax + dx * b->rx + dy * b->ry;
    *sy = (float)cam->ay - (dx * b->fx + dy * b->fy);
}

/* Remembers the last tile so runs of pixels in one tile skip the lookup. */
typedef struct {
    int32_t cx;
    int32_t cy;
    const uint8_t *px;
} TileMemo;

static uint8_t class_at(const RnPack *pack, RnTileCache *cache, TileMemo *m, int32_t ix, int32_t iy)
{
    if (ix >= 0 && iy >= 0) {
        int32_t cx = ix / (int32_t)RN_TILE;
        int32_t cy = iy / (int32_t)RN_TILE;
        if (cx != m->cx || cy != m->cy) {
            uint16_t slot = rn_pack_slot(pack, cx, cy);
            m->cx = cx;
            m->cy = cy;
            m->px = (slot == (uint16_t)RN_PACK_NO_TILE) ? NULL : rn_tile_cache_get(cache, pack, slot);
        }
        if (m->px != NULL) {
            uint32_t i = (((uint32_t)iy & 31u) << 5) | ((uint32_t)ix & 31u);
            uint8_t b = m->px[i >> 1];
            return ((i & 1u) != 0u) ? (uint8_t)(b & 0x0Fu) : (uint8_t)(b >> 4);
        }
    }
    bool dot = (((uint32_t)ix & 7u) == 0u) && (((uint32_t)iy & 7u) == 0u);
    return dot ? (uint8_t)RT_C_BUILDING : (uint8_t)RT_C_LAND;
}

void rt_map_draw(RtCanvas *cv, const RnPack *pack, RnTileCache *cache, const RtCamera *cam)
{
    if (!RN_ASSERT(cv != NULL && pack != NULL && cache != NULL && cam != NULL)) {
        return;
    }
    Basis b = basis(cam->heading);
    int32_t sx_x = (int32_t)(b.rx * FIX_ONE);       /* step per screen x */
    int32_t sx_y = (int32_t)(b.ry * FIX_ONE);
    int32_t sy_x = (int32_t)(-b.fx * FIX_ONE);      /* step per screen y */
    int32_t sy_y = (int32_t)(-b.fy * FIX_ONE);
    float ox = cam->x - (float)cam->ax * b.rx + (float)cam->ay * b.fx;
    float oy = cam->y - (float)cam->ax * b.ry + (float)cam->ay * b.fy;
    if (!(fabsf(ox) < 1.0e6f && fabsf(oy) < 1.0e6f)) {   /* rider far off the map (or NaN) */
        rt_canvas_clear(cv, RT_C_LAND);
        return;
    }
    /* 16.16 fixed point relative to (bx, by), so big packs never overflow int32 */
    int32_t bx = (int32_t)floorf(ox);
    int32_t by = (int32_t)floorf(oy);
    int32_t row_x = (int32_t)((ox - (float)bx) * FIX_ONE);
    int32_t row_y = (int32_t)((oy - (float)by) * FIX_ONE);
    TileMemo memo = {-1, -1, NULL};
    for (int32_t y = 0; y < RT_SCREEN_H; y++) {
        int32_t wx = row_x;
        int32_t wy = row_y;
        uint8_t *out = &cv->px[y * RT_STRIDE];
        for (int32_t x = 0; x < RT_STRIDE; x++) {
            uint8_t hi = class_at(pack, cache, &memo, bx + (wx >> 16), by + (wy >> 16));
            wx += sx_x;
            wy += sx_y;
            uint8_t lo = class_at(pack, cache, &memo, bx + (wx >> 16), by + (wy >> 16));
            wx += sx_x;
            wy += sx_y;
            out[x] = (uint8_t)((hi << 4) | lo);
        }
        row_x += sy_x;
        row_y += sy_y;
    }
}

typedef struct {
    int32_t radius;
    uint8_t color;
    bool done_part;    /* draw the part behind the rider  */
    bool ahead_part;   /* draw the part ahead of it       */
} RoutePass;

static void route_pass(RtCanvas *cv, const RnLine *line, const RtCamera *cam, const Basis *b,
                       float along, const RoutePass *ps)
{
    RnPackPoint here = rn_line_position_at(line, along);
    float hx = 0.0f, hy = 0.0f, px = 0.0f, py = 0.0f;
    to_screen(cam, b, here.x, here.y, &hx, &hy);
    bool prev_ahead = false;
    uint32_t n = rn_line_count(line);
    for (uint32_t i = 0u; i < n && i < RN_MAX_ROUTE_PTS; i++) {
        RnPackPoint pt = rn_line_point(line, i);
        float sx = 0.0f, sy = 0.0f;
        to_screen(cam, b, pt.x, pt.y, &sx, &sy);
        bool ahead = pt.along > along;
        if (i > 0u) {
            if (!prev_ahead && ahead) {                 /* the segment the rider is on */
                if (ps->done_part) {
                    rt_canvas_thick_line(cv, px, py, hx, hy, ps->radius, ps->color);
                }
                if (ps->ahead_part) {
                    rt_canvas_thick_line(cv, hx, hy, sx, sy, ps->radius, ps->color);
                }
            } else if ((ahead && ps->ahead_part) || (!ahead && ps->done_part)) {
                rt_canvas_thick_line(cv, px, py, sx, sy, ps->radius, ps->color);
            } else {
                /* not drawn in this pass */
            }
        }
        px = sx;
        py = sy;
        prev_ahead = ahead;
    }
}

void rt_map_line(RtCanvas *cv, const RnLine *line, const RtCamera *cam, float along, float next_along)
{
    if (!RN_ASSERT(cv != NULL && line != NULL && cam != NULL)) {
        return;
    }
    Basis b = basis(cam->heading);
    const RoutePass casing = {4, RT_C_SHADOW, false, true};
    const RoutePass done = {1, RT_C_ROUTE_DONE, true, false};
    const RoutePass ahead = {3, RT_C_ROUTE, false, true};
    route_pass(cv, line, cam, &b, along, &casing);
    route_pass(cv, line, cam, &b, along, &done);
    route_pass(cv, line, cam, &b, along, &ahead);
    if (next_along > along) {
        RnPackPoint m = rn_line_position_at(line, next_along);
        float mx = 0.0f, my = 0.0f;
        to_screen(cam, &b, m.x, m.y, &mx, &my);
        rt_canvas_fill_circle(cv, (int32_t)mx, (int32_t)my, 5, RT_C_SHADOW);
        rt_canvas_fill_circle(cv, (int32_t)mx, (int32_t)my, 3, RT_C_TEXT);
    }
}

void rt_map_route(RtCanvas *cv, const RnPack *pack, const RtCamera *cam, float along, float next_along)
{
    if (!RN_ASSERT(pack != NULL)) {
        return;
    }
    RnLine line = rn_line_from_pack(pack);
    rt_map_line(cv, &line, cam, along, next_along);
}

static void arrow(RtCanvas *cv, const RtCamera *cam, float rel_heading, float s, uint8_t col)
{
    float fx = sinf(rel_heading), fy = -cosf(rel_heading);
    float rx = -fy, ry = fx;
    float ax = (float)cam->ax, ay = (float)cam->ay;
    float tip_x = ax + fx * 10.0f * s, tip_y = ay + fy * 10.0f * s;
    float lx = ax - fx * 7.0f * s - rx * 8.0f * s, ly = ay - fy * 7.0f * s - ry * 8.0f * s;
    float qx = ax - fx * 7.0f * s + rx * 8.0f * s, qy = ay - fy * 7.0f * s + ry * 8.0f * s;
    float nx = ax - fx * 2.0f * s, ny = ay - fy * 2.0f * s;
    const float left_x[3] = {tip_x, lx, nx}, left_y[3] = {tip_y, ly, ny};
    const float right_x[3] = {tip_x, nx, qx}, right_y[3] = {tip_y, ny, qy};
    rt_canvas_fill_triangle(cv, left_x, left_y, col);
    rt_canvas_fill_triangle(cv, right_x, right_y, col);
}

void rt_map_rider(RtCanvas *cv, const RtCamera *cam, float rider_heading)
{
    if (!RN_ASSERT(cv != NULL && cam != NULL)) {
        return;
    }
    float rel = rider_heading - cam->heading;
    arrow(cv, cam, rel, 1.45f, RT_C_SHADOW);
    arrow(cv, cam, rel, 1.0f, RT_C_HILITE);
}
