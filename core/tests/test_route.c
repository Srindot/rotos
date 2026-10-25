/*
 * test_route.c - route model, maneuvers, lookahead.
 */
#include <math.h>
#include <string.h>

#include "rn/rn_route.h"
#include "rn_test.h"
#include "test_fixtures.h"

void test_route(void)
{
    (void)printf("route\n");
    static RnRoute r;
    build_l_route(&r);
    CHECK(r.n_pts == 21u);
    CHECK(r.total_m > 1990.0f && r.total_m < 2010.0f);
    CHECK(r.n_man == 3u);
    CHECK(r.man[2].type == RN_M_ARRIVE);
    CHECK(strcmp(r.man[1].road, "ROAD NO. 2") == 0);
    CHECK_NEAR(rn_route_heading_at(&r, 500.0f), 0.0f, 0.02f);              /* north */
    CHECK_NEAR(rn_route_heading_at(&r, 1500.0f), RN_PI_F / 2.0f, 0.02f);   /* east  */
    RnVec2 p = rn_route_position_at(&r, 250.0f);
    CHECK(fabsf(p.x) < 1.0f && p.y > 245.0f && p.y < 255.0f);
    RnVec2 end = rn_route_position_at(&r, 1e9f);
    CHECK(end.x > 990.0f);

    float along[3];
    uint8_t types[3];
    for (uint32_t i = 0u; i < 3u; i++) {
        along[i] = r.man[i].along_m;
        types[i] = r.man[i].type;
    }
    RnLookahead la = rn_maneuver_lookahead(along, types, 3u, 100.0f);
    CHECK(la.current == 0 && la.next == 1 && la.then == -1);
    la = rn_maneuver_lookahead(along, types, 3u, 1900.0f);
    CHECK(la.next == 2 && la.then == -1);
    CHECK(rn_maneuver_from_google("TURN_SHARP_RIGHT") == RN_M_SHARP_RIGHT);
    CHECK(rn_maneuver_from_google("SOMETHING_NEW") == RN_M_STRAIGHT);
}
