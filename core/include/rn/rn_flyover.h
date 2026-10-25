/*
 * rn_flyover.h - flyover vs service-road hints.
 *
 * Google says "slight right" - or nothing at all - where a flyover splits
 * from the road below, which is exactly where riders go wrong. Using OSM
 * bridge geometry (Overpass API: highway ways with bridge=yes) maneuvers are
 * tagged TAKE / STAY_BELOW / EXIT, and a STRAIGHT maneuver is added where
 * Google gives no instruction but a decision exists.
 */
#ifndef RN_FLYOVER_H
#define RN_FLYOVER_H

#include "rn/rn_route.h"

#define RN_FLY_MIN_BRIDGE_M   120.0f   /* shorter "bridges" are culverts       */
#define RN_FLY_STEP_M           5.0f   /* route sampling step                  */
#define RN_FLY_MAX_SAMPLES    40002u   /* RN_MAX_ROUTE_M / RN_FLY_STEP_M + 2 */

typedef struct {
    RnVec2 pts[RN_MAX_BRIDGE_PTS];          /* route-local metres          */
    uint16_t start[RN_MAX_BRIDGES + 1u];    /* bridge i = pts[start[i] .. start[i+1]) */
    uint32_t n_bridges;
    uint32_t n_pts;
} RnBridgeSet;

typedef struct {
    uint8_t on_bridge[RN_FLY_MAX_SAMPLES];  /* route runs along a bridge   */
    uint8_t start_near[RN_FLY_MAX_SAMPLES]; /* a bridge starts here our way */
    uint32_t n;
} RnFlyoverWork;

void rn_bridges_clear(RnBridgeSet *b);
/* Add one OSM way (lat/lon vertices). Returns RN_OK when stored, RN_ERR_RANGE
 * when skipped as too short, RN_ERR_FULL when out of space. */
rn_status rn_bridges_add(RnBridgeSet *b, const RnRoute *r, const double *lat,
                         const double *lon, uint32_t n);

/* Tag maneuvers and insert synthetic ones. Returns the number of hints set
 * (>= 0) or a negative rn_status. */
int32_t rn_flyover_annotate(RnRoute *r, const RnBridgeSet *b, RnFlyoverWork *w);

/* Offline fallback: hints from road names containing "FLYOVER". */
int32_t rn_flyover_by_name(RnRoute *r);

#endif /* RN_FLYOVER_H */
