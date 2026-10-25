/*
 * test_fixtures.h - shared synthetic data: an L-shaped 2 km route in
 * Hyderabad (1 km north, then 1 km east, vertices every 100 m).
 */
#ifndef TEST_FIXTURES_H
#define TEST_FIXTURES_H

#include "rn/rn_route.h"

#define FIX_LAT0 17.4300
#define FIX_LON0 78.3500

static inline void build_l_route(RnRoute *r)
{
    rn_route_clear(r);
    RnLocalFrame f = rn_geo_frame(FIX_LAT0, FIX_LON0);
    for (int i = 0; i <= 10; i++) {
        RnVec2 p = {0.0f, (float)i * 100.0f};
        double la = 0.0;
        double lo = 0.0;
        rn_geo_from_local(&f, p, &la, &lo);
        (void)rn_route_add_point(r, la, lo);
    }
    for (int i = 1; i <= 10; i++) {
        RnVec2 p = {(float)i * 100.0f, 1000.0f};
        double la = 0.0;
        double lo = 0.0;
        rn_geo_from_local(&f, p, &la, &lo);
        (void)rn_route_add_point(r, la, lo);
    }
    (void)rn_route_add_maneuver(r, RN_M_DEPART, 0u, "Road No. 1");
    (void)rn_route_add_maneuver(r, RN_M_RIGHT, 10u, "Road No. 2");
    (void)rn_route_finish(r, 300.0f, 0x1234u);
}

#endif
