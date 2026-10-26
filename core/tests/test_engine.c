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

/* A loop ride (home -> 1 km square -> home): standing at home is the start, not "arrived". */
static void test_loop_ride_starts_at_the_start(void)
{
    static RnRoute r;
    rn_route_clear(&r);
    RnLocalFrame f = rn_geo_frame(FIX_LAT0, FIX_LON0);
    static const float CORNERS[5][2] = {{0.0f, 0.0f}, {0.0f, 1000.0f}, {1000.0f, 1000.0f}, {1000.0f, 0.0f}, {0.0f, 0.0f}};
    for (int c = 0; c < 4; c++) {
        for (int k = 0; k < 10; k++) {
            RnVec2 p = {CORNERS[c][0] + (CORNERS[c + 1][0] - CORNERS[c][0]) * (float)k / 10.0f,
                        CORNERS[c][1] + (CORNERS[c + 1][1] - CORNERS[c][1]) * (float)k / 10.0f};
            double la = 0.0;
            double lo = 0.0;
            rn_geo_from_local(&f, p, &la, &lo);
            (void)rn_route_add_point(&r, la, lo);
        }
    }
    double la0 = 0.0;
    double lo0 = 0.0;
    rn_geo_from_local(&f, (RnVec2){0.0f, 0.0f}, &la0, &lo0);
    (void)rn_route_add_point(&r, la0, lo0);
    (void)rn_route_finish(&r, 600.0f, 0x55u);
    RnEngine e;
    rn_engine_init(&e);
    RnNavState ns;
    RnFix fix = {1000u, 1791613800.0, 19800, 0.0, 0.0, 0.0f, 0.0f, 5.0f};
    rn_geo_from_local(&f, (RnVec2){3.0f, 0.0f}, &fix.lat, &fix.lon);    /* 3 m from home, standing */
    for (uint32_t k = 0u; k < 5u; k++) {
        fix.t_ms = 1000u * (k + 1u);
        CHECK(rn_engine_on_fix(&e, &r, &fix, &ns) == RN_OK);
    }
    CHECK((ns.flags & RN_NAV_ARRIVED) == 0u);
    CHECK(ns.along_m < 50.0f);                                       /* locked to the start */
}

/* A short leg first locked near its end (the rider moved on while it was built) still arrives. */
static void test_short_route_locked_near_the_end(void)
{
    static RnRoute r;
    rn_route_clear(&r);
    RnLocalFrame f = rn_geo_frame(FIX_LAT0, FIX_LON0);
    for (int k = 0; k <= 6; k++) {
        double la = 0.0;
        double lo = 0.0;
        rn_geo_from_local(&f, (RnVec2){0.0f, 50.0f * (float)k}, &la, &lo);
        (void)rn_route_add_point(&r, la, lo);
    }
    (void)rn_route_finish(&r, 60.0f, 0x66u);
    RnEngine e;
    rn_engine_init(&e);
    RnNavState ns;
    RnFix fix = {1000u, 1791613800.0, 19800, 0.0, 0.0, 0.0f, 0.0f, 5.0f};
    rn_geo_from_local(&f, (RnVec2){0.0f, 297.0f}, &fix.lat, &fix.lon);
    for (uint32_t k = 0u; k < 3u; k++) {
        fix.t_ms = 1000u * (k + 1u);
        CHECK(rn_engine_on_fix(&e, &r, &fix, &ns) == RN_OK);
    }
    CHECK((ns.flags & RN_NAV_ARRIVED) != 0u);
}

/* Out and back on carriageways 30 m apart: a rider on the way back locks to the way back. */
static void test_out_and_back_locks_to_the_nearer_leg(void)
{
    static RnRoute r;
    rn_route_clear(&r);
    RnLocalFrame f = rn_geo_frame(FIX_LAT0, FIX_LON0);
    for (int k = 0; k <= 20; k++) {                       /* north 2 km on x = 0 */
        double la = 0.0;
        double lo = 0.0;
        rn_geo_from_local(&f, (RnVec2){0.0f, 100.0f * (float)k}, &la, &lo);
        (void)rn_route_add_point(&r, la, lo);
    }
    for (int k = 20; k >= 0; k--) {                       /* and back south on x = 30 */
        double la = 0.0;
        double lo = 0.0;
        rn_geo_from_local(&f, (RnVec2){30.0f, 100.0f * (float)k}, &la, &lo);
        (void)rn_route_add_point(&r, la, lo);
    }
    (void)rn_route_finish(&r, 600.0f, 0x77u);
    RnEngine e;
    rn_engine_init(&e);
    RnNavState ns;
    RnFix fix = {1000u, 1791613800.0, 19800, 0.0, 0.0, 0.0f, 0.0f, 5.0f};
    rn_geo_from_local(&f, (RnVec2){31.0f, 500.0f}, &fix.lat, &fix.lon);   /* on the way back, 1.5 km in */
    CHECK(rn_engine_on_fix(&e, &r, &fix, &ns) == RN_OK);
    CHECK(ns.along_m > 3000.0f);
}

/* Back onto a densely sampled route with a 13 m GPS offset (tunnel exit): it locks again. */
static void test_relock_with_offset_on_dense_route(void)
{
    static RnRoute r;
    rn_route_clear(&r);
    RnLocalFrame f = rn_geo_frame(FIX_LAT0, FIX_LON0);
    for (int k = 0; k <= 200; k++) {                          /* 2 km north, a vertex every 10 m */
        double la = 0.0;
        double lo = 0.0;
        rn_geo_from_local(&f, (RnVec2){0.0f, 10.0f * (float)k}, &la, &lo);
        (void)rn_route_add_point(&r, la, lo);
    }
    (void)rn_route_finish(&r, 300.0f, 0x88u);
    RnEngine e;
    rn_engine_init(&e);
    RnNavState ns;
    RnFix fix = {1000u, 1791613800.0, 19800, 0.0, 0.0, 10.0f, 0.0f, 5.0f};
    uint32_t t = 0u;
    for (int k = 0; k < 5; k++) {                             /* locked on the road */
        rn_geo_from_local(&f, (RnVec2){0.0f, 100.0f + 10.0f * (float)k}, &fix.lat, &fix.lon);
        fix.t_ms = (t += 1000u);
        CHECK(rn_engine_on_fix(&e, &r, &fix, &ns) == RN_OK);
    }
    for (int k = 0; k < 4; k++) {                             /* tunnel: wild fixes, off route */
        rn_geo_from_local(&f, (RnVec2){200.0f, 150.0f + 10.0f * (float)k}, &fix.lat, &fix.lon);
        fix.t_ms = (t += 1000u);
        CHECK(rn_engine_on_fix(&e, &r, &fix, &ns) == RN_OK);
    }
    for (int k = 0; k < 6; k++) {                             /* out of the tunnel, 13 m off */
        rn_geo_from_local(&f, (RnVec2){13.0f, 600.0f + 10.0f * (float)k}, &fix.lat, &fix.lon);
        fix.t_ms = (t += 1000u);
        CHECK(rn_engine_on_fix(&e, &r, &fix, &ns) == RN_OK);
    }
    CHECK(ns.along_m > 600.0f && ns.along_m < 700.0f);        /* locked again, at the rider */
    CHECK((ns.flags & RN_NAV_OFF_ROUTE) == 0u);
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
    test_loop_ride_starts_at_the_start();
    test_relock_with_offset_on_dense_route();
    test_short_route_locked_near_the_end();
    test_out_and_back_locks_to_the_nearer_leg();
    test_off_route(&r);
    test_wrong_way_carriageway(&r);
}
