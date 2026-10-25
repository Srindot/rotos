/*
 * rn_line.c - a route line from the pack or from RAM.
 */
#include "rn/rn_line.h"

#include <math.h>

#define BSEARCH_STEPS 32u

RnLine rn_line_from_pack(const RnPack *p)
{
    RnLine l = {p, NULL, 0u};
    if (RN_ASSERT(p != NULL) && p->blob != NULL) {
        l.n = (p->h.n_points < RN_MAX_ROUTE_PTS) ? p->h.n_points : RN_MAX_ROUTE_PTS;
    }
    return l;
}

RnLine rn_line_from_points(const RnPackPoint *pts, uint32_t n)
{
    RnLine l = {NULL, pts, 0u};
    if (RN_ASSERT(pts != NULL || n == 0u)) {
        l.n = (n < RN_LINE_MAX_PTS) ? n : RN_LINE_MAX_PTS;
    }
    return l;
}

uint32_t rn_line_count(const RnLine *l)
{
    return RN_ASSERT(l != NULL) ? l->n : 0u;
}

RnPackPoint rn_line_point(const RnLine *l, uint32_t i)
{
    RnPackPoint none = {0.0f, 0.0f, 0.0f};
    if (!RN_ASSERT(l != NULL) || i >= l->n) {
        return none;
    }
    return (l->pts != NULL) ? l->pts[i] : rn_pack_point(l->pack, i);
}

float rn_line_total(const RnLine *l)
{
    return (l != NULL && l->n > 0u) ? rn_line_point(l, l->n - 1u).along : 0.0f;
}

RnPackPoint rn_line_position_at(const RnLine *l, float along)
{
    RnPackPoint first = rn_line_point(l, 0u);
    if (l == NULL || l->n < 2u || along <= first.along) {
        return first;
    }
    RnPackPoint last = rn_line_point(l, l->n - 1u);
    if (along >= last.along) {
        return last;
    }
    uint32_t lo = 0u;
    uint32_t hi = l->n - 1u;
    for (uint32_t k = 0u; k < BSEARCH_STEPS && hi - lo > 1u; k++) {
        uint32_t mid = (lo + hi) / 2u;
        if (rn_line_point(l, mid).along <= along) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    RnPackPoint a = rn_line_point(l, lo);
    RnPackPoint b = rn_line_point(l, hi);
    float t = (along - a.along) / fmaxf(b.along - a.along, 1e-3f);
    RnPackPoint out = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, along};
    return out;
}
