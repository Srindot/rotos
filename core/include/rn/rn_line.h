/*
 * rn_line.h - the route line ROTOS draws and the phone follows: either the
 * route stored in the map pack, or a newer one received at run time (ROUTE
 * packets, e.g. after Google Maps rerouted) in the same pack pixels, so a
 * reroute only sends a few KB instead of a whole map.
 */
#ifndef RN_LINE_H
#define RN_LINE_H

#include "rn/rn_mappack.h"

#define RN_LINE_MAX_PTS 1000u          /* the phone simplifies longer routes */

typedef struct {
    const RnPack *pack;                /* pack route when pts == NULL     */
    const RnPackPoint *pts;            /* or points in RAM                */
    uint32_t n;
} RnLine;

RnLine rn_line_from_pack(const RnPack *p);
RnLine rn_line_from_points(const RnPackPoint *pts, uint32_t n);
uint32_t rn_line_count(const RnLine *l);
RnPackPoint rn_line_point(const RnLine *l, uint32_t i);
/* Position `along` metres from the start (clamped to the ends). */
RnPackPoint rn_line_position_at(const RnLine *l, float along);
/* Length in metres (along of the last point). */
float rn_line_total(const RnLine *l);

#endif /* RN_LINE_H */
