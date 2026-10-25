/*
 * rt_routerx.c - route lines received at run time.
 */
#include "rt_routerx.h"

#include <string.h>

#include "rn/rn_types.h"

void rt_routerx_init(RtRouteRx *r, RnPackPoint *buf)
{
    if (!RN_ASSERT(r != NULL)) {
        return;
    }
    (void)memset(r, 0, sizeof *r);
    r->pts = buf;
}

void rt_routerx_forget(RtRouteRx *r)
{
    if (RN_ASSERT(r != NULL)) {
        r->valid = false;
        r->got = 0u;
        r->route_id = 0u;
        (void)memset(r->seen, 0, sizeof r->seen);
    }
}

/* A chunk of a new line: start over (the old line stops being drawn). */
static void start(RtRouteRx *r, const RnRouteChunk *c)
{
    rt_routerx_forget(r);
    r->route_id = c->route_id;
    r->pack_route_id = c->pack_route_id;
    r->total = c->total;
}

bool rt_routerx_on(RtRouteRx *r, const RnRouteChunk *c)
{
    if (!RN_ASSERT(r != NULL && c != NULL) || r->pts == NULL) {
        return false;
    }
    if (c->route_id != r->route_id || c->total != r->total || c->pack_route_id != r->pack_route_id) {
        start(r, c);
    }
    uint32_t k = (uint32_t)c->first / RN_ROUTE_CHUNK_PTS;
    bool last = (uint32_t)c->first + c->n >= r->total;
    bool aligned = (c->first % RN_ROUTE_CHUNK_PTS) == 0u && k < RT_ROUTE_CHUNKS;
    if (!aligned || (r->seen[k / 8u] & (uint8_t)(1u << (k % 8u))) != 0u) {
        return aligned && r->valid && last;          /* a resent line: confirm it again */
    }
    for (uint32_t i = 0u; i < c->n; i++) {
        RnPackPoint p = {c->x[i], c->y[i], c->along[i]};
        r->pts[c->first + i] = p;
    }
    r->seen[k / 8u] |= (uint8_t)(1u << (k % 8u));
    r->got += c->n;
    bool done = !r->valid && r->got >= r->total;
    r->valid = r->valid || done;
    return done;
}

bool rt_routerx_line(const RtRouteRx *r, uint32_t pack_route_id, RnLine *out)
{
    if (!RN_ASSERT(r != NULL && out != NULL) || !r->valid || r->pack_route_id != pack_route_id ||
        r->total < 2u) {
        return false;
    }
    *out = rn_line_from_points(r->pts, r->total);
    return true;
}
