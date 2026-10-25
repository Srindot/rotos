/*
 * rn_route.h - the phone-side route model: geometry in lat/lon and in local
 * metres, cumulative distance, and the maneuver list.
 *
 * Built once per route from Google (or OSRM) data; every other phone-side
 * algorithm (matching, flyover hints, corridor, ride simulation) reads it.
 * About 115 KB: the phone, the simulator and the JNI layer keep one static
 * instance; the ESP32 never holds one (it reads the map pack instead).
 */
#ifndef RN_ROUTE_H
#define RN_ROUTE_H

#include "rn/rn_geo.h"
#include "rn/rn_maneuver.h"
#include "rn/rn_types.h"

#define RN_ROUTE_MIN_STEP_M  2.0    /* closer route points are dropped */

typedef struct {
    double lat;
    double lon;
    RnVec2 xy;        /* local metres in route->frame */
    float along_m;    /* cumulative distance from the first point */
} RnRoutePoint;

typedef struct {
    RnRoutePoint pts[RN_MAX_ROUTE_PTS];
    uint32_t n_pts;
    RnManeuver man[RN_MAX_MANEUVERS];
    uint32_t n_man;
    RnLocalFrame frame;
    float total_m;
    float duration_s;     /* routing engine's travel time estimate */
    uint32_t route_id;    /* identifies the map pack built from it */
} RnRoute;

/* Reset to an empty route. */
void rn_route_clear(RnRoute *r);

/* Append geometry. Consecutive duplicates are skipped. The local frame is
 * anchored at the first point ever added. */
rn_status rn_route_add_point(RnRoute *r, double lat, double lon);

/* Add a maneuver at route vertex point_idx; road may be NULL. Maneuvers must
 * be added in route order. */
rn_status rn_route_add_maneuver(RnRoute *r, uint8_t type, uint32_t point_idx, const char *road);

/* Close the route: appends ARRIVE if missing, sets totals. */
rn_status rn_route_finish(RnRoute *r, float duration_s, uint32_t route_id);

/* Position (local metres) and heading (radians, 0 = north, clockwise) at
 * distance s along the route, clamped to [0, total]. */
RnVec2 rn_route_position_at(const RnRoute *r, float s);
float rn_route_heading_at(const RnRoute *r, float s);
void rn_route_latlon_at(const RnRoute *r, float s, double *lat, double *lon);

/* Index i such that pts[i].along <= s < pts[i+1].along (binary search). */
uint32_t rn_route_segment_at(const RnRoute *r, float s);

/* Insert a maneuver keeping the list sorted by along_m (used for synthetic
 * flyover maneuvers). */
rn_status rn_route_insert_maneuver(RnRoute *r, const RnManeuver *m);

#endif /* RN_ROUTE_H */
