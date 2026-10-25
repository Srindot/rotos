/*
 * rn_corridor.h - which map tiles to keep for a route: the grid of 32 px
 * cells (at the pack zoom) whose centre lies within a buffer of the route.
 * Only those cells are fetched from OSM, quantized and stored, so a 7 km
 * route costs ~1.5 k tiles instead of the ~3.3 k in its bounding box.
 */
#ifndef RN_CORRIDOR_H
#define RN_CORRIDOR_H

#include "rn/rn_route.h"

#define RN_CORRIDOR_MAX_SIDE   1024u                     /* cells per side */
#define RN_CORRIDOR_MAX_CELLS  (RN_CORRIDOR_MAX_SIDE * RN_CORRIDOR_MAX_SIDE)

typedef struct {
    uint8_t zoom;
    uint32_t buffer_px;
    int32_t origin_x;          /* world px (pack zoom) of cell (0, 0)    */
    int32_t origin_y;
    uint16_t tiles_w;
    uint16_t tiles_h;
    uint32_t n_keep;
    float m_per_px;
    uint8_t keep[RN_CORRIDOR_MAX_CELLS / 8u];
} RnCorridor;

/* Plan the corridor of route r at `zoom` with a half-width of buffer_px. */
rn_status rn_corridor_plan(RnCorridor *c, const RnRoute *r, uint8_t zoom, uint32_t buffer_px);
bool rn_corridor_keep(const RnCorridor *c, uint32_t cx, uint32_t cy);

/* Route vertex i in pack pixels (relative to the corridor origin). */
RnVec2 rn_corridor_point_px(const RnCorridor *c, const RnRoute *r, uint32_t i);

#endif /* RN_CORRIDOR_H */
