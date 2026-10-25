/*
 * rt_routerx.h - receives a route line (ROUTE packets) into RAM: a reroute
 * replaces only the line ROTOS draws, in a fraction of a second, while the
 * map stays (docs/PROTOCOL.md, ROUTE).
 */
#ifndef RT_ROUTERX_H
#define RT_ROUTERX_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_line.h"
#include "rn/rn_protocol.h"

#define RT_ROUTE_CHUNKS ((RN_ROUTE_MAX_PTS + RN_ROUTE_CHUNK_PTS - 1u) / RN_ROUTE_CHUNK_PTS)

typedef struct {
    RnPackPoint *pts;                /* RN_ROUTE_MAX_PTS, owned by main      */
    uint32_t route_id;               /* line being received / shown          */
    uint32_t pack_route_id;          /* the pack whose pixels it uses        */
    uint32_t total;
    uint32_t got;                    /* points received so far               */
    uint8_t seen[(RT_ROUTE_CHUNKS + 7u) / 8u];
    bool valid;                      /* every point arrived                  */
} RtRouteRx;

/* buf: RN_ROUTE_MAX_PTS points (NULL disables run-time lines). */
void rt_routerx_init(RtRouteRx *r, RnPackPoint *buf);
/* One ROUTE chunk; true when the line is complete after it and the ACK should
 * go out (the chunk that completed it, or the last chunk of a resent line). */
bool rt_routerx_on(RtRouteRx *r, const RnRouteChunk *c);
/* The line to draw for the pack with route id pack_route_id, if there is one. */
bool rt_routerx_line(const RtRouteRx *r, uint32_t pack_route_id, RnLine *out);
void rt_routerx_forget(RtRouteRx *r);

#endif /* RT_ROUTERX_H */
