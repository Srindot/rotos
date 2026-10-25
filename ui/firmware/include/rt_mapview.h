/*
 * rt_mapview.h - the map: rotated raster of map classes from the map pack,
 * the route on top (ridden part thin and dim), the next maneuver point, and
 * the rider marker.
 */
#ifndef RT_MAPVIEW_H
#define RT_MAPVIEW_H

#include "rn/rn_line.h"
#include "rn/rn_mappack.h"
#include "rt_canvas.h"

typedef struct {
    float x;          /* pack px under the anchor                       */
    float y;
    float heading;    /* radians; the map is rotated so this points up  */
    int32_t ax;       /* anchor on screen (where the rider is)          */
    int32_t ay;
} RtCamera;

/* Inverse-mapped raster with 16.16 fixed-point steps: per pixel two adds and
 * a nibble read - cheap enough for an ESP32-C3 without an FPU. Outside the
 * stored corridor a sparse dot grid is drawn, like the void of a Game Boy map. */
void rt_map_draw(RtCanvas *cv, const RnPack *pack, RnTileCache *cache, const RtCamera *cam);
void rt_map_route(RtCanvas *cv, const RnPack *pack, const RtCamera *cam, float along, float next_along);
/* Same for any route line (a run-time ROUTE line, or the pack's). */
void rt_map_line(RtCanvas *cv, const RnLine *line, const RtCamera *cam, float along, float next_along);
void rt_map_rider(RtCanvas *cv, const RtCamera *cam, float rider_heading);

#endif /* RT_MAPVIEW_H */
