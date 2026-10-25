/*
 * test_engine.c - map matching, engine flags, ride simulator, tracker.
 */
#include <math.h>
#include <string.h>

#include "rn/rn_engine.h"
#include "rn/rn_ridesim.h"
#include "rn_test.h"
#include "test_fixtures.h"

static void test_full_ride(const RnRoute *r)
{
    static RnRideSim sim;
    RnEngine e;
    rn_engine_init(&e);
    rn_ridesim_init(&sim, r, 7u, RN_SIM_CRUISE_MPS);
    float prev = -1.0f;
    int backwards = 0;
    int off = 0;
    bool arrived = false;
    uint32_t t_ms = 0u;
    for (int k = 0; k < 2000 && !arrived; k++) {
        for (int s = 0; s < 20; s++) {
            (void)rn_ridesim_step(&sim, r, 0.05f);
        }
        t_ms += 1000u;
        RnFix fix;
        rn_ridesim_fix(&sim, r, 3.0f, t_ms, 1791613800.0 + (double)t_ms / 1000.0, 19800, &fix);
        RnNavState ns;
        CHECK(rn_engine_on_fix(&e, r, &fix, &ns) == RN_OK);
        backwards += (ns.along_m < prev - 10.0f);
        off += (ns.flags & RN_NAV_OFF_ROUTE) != 0u;
        prev = ns.along_m;
        arrived = (ns.flags & RN_NAV_ARRIVED) != 0u;
    }
    CHECK(arrived);
    CHECK(backwards == 0);
    CHECK(off == 0);
    CHECK(sim.t > 150.0f && sim.t < 400.0f);                  /* 2 km incl. a red light */
}

static void test_off_route(const RnRoute *r)
{
    RnEngine e;
    rn_engine_init(&e);
    RnNavState ns;
    RnFix fix = {1000u, 1791613800.0, 19800, 0.0, 0.0, 10.0f, 0.0f, 5.0f};
    for (uint32_t k = 0u; k < 15u; k++) {
        RnVec2 p = {(k < 2u) ? 0.0f : 150.0f, 100.0f + 10.0f * (float)k};  /* drifts 150 m east */
        rn_geo_from_local(&r->frame, p, &fix.lat, &fix.lon);
        fix.t_ms = 1000u * (k + 1u);
        CHECK(rn_engine_on_fix(&e, r, &fix, &ns) == RN_OK);
        if (k == 3u) {
            CHECK((ns.flags & RN_NAV_OFF_ROUTE) == 0u);       /* 2 bad fixes: not yet */
        }
    }
    CHECK((ns.flags & RN_NAV_OFF_ROUTE) != 0u);
    CHECK(rn_engine_wants_reroute(&e, fix.t_ms));
    CHECK(ns.along_m < 200.0f);                              /* held, no jump */
}

static void test_wrong_way_carriageway(const RnRoute *r)
{
    /* A fix exactly at the corner heading east must match the east leg. */
    RnMatcher m;
    rn_matcher_init(&m);
    RnMatchInput in;
    memset(&in, 0, sizeof in);
    in.pos.x = 30.0f;
    in.pos.y = 990.0f;
    in.speed_mps = 10.0f;
    in.heading_deg = 90.0f;
    in.heading_valid = true;
    in.accuracy_m = 5.0f;
    CHECK(rn_matcher_update(&m, r, &in) == RN_OK);
    CHECK(m.along_m > 1020.0f && m.along_m < 1040.0f);
}

void test_engine(void)
{
    (void)printf("engine\n");
    static RnRoute r;
    build_l_route(&r);
    test_full_ride(&r);
    test_off_route(&r);
    test_wrong_way_carriageway(&r);
}
