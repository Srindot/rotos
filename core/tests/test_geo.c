/*
 * test_geo.c - geodesy, polyline codec, text helpers, sun.
 */
#include <string.h>

#include "rn/rn_geo.h"
#include "rn/rn_polyline.h"
#include "rn/rn_sun.h"
#include "rn/rn_text.h"
#include "rn_test.h"

static void test_polyline(void)
{
    /* Google's documented example */
    double lat[8];
    double lon[8];
    CHECK(rn_polyline_decode("_qxeO?_qxeO?", lat, lon, 8u) == RN_ERR_FORMAT);   /* 85 + 85 degrees */
    int32_t n = rn_polyline_decode("_p~iF~ps|U_ulLnnqC_mqNvxq`@", lat, lon, 8u);
    CHECK(n == 3);
    CHECK_NEAR(lat[0], 38.5, 1e-9);
    CHECK_NEAR(lon[0], -120.2, 1e-9);
    CHECK_NEAR(lat[2], 43.252, 1e-9);
    CHECK_NEAR(lon[2], -126.453, 1e-9);
    char enc[64];
    CHECK(rn_polyline_encode(lat, lon, 3u, enc, sizeof enc) > 0);
    CHECK(strcmp(enc, "_p~iF~ps|U_ulLnnqC_mqNvxq`@") == 0);
    CHECK(rn_polyline_decode("_p~iF~ps|U_ulLnnqC_mqNvxq`@", lat, lon, 2u) == RN_ERR_FULL);
    CHECK(rn_polyline_decode("_p~iF", lat, lon, 8u) == RN_ERR_FORMAT);
}

static void test_distances(void)
{
    double d = rn_geo_haversine_m(17.4455, 78.3489, 17.4300, 78.3890);
    CHECK(d > 4400.0 && d < 4600.0);
    CHECK_NEAR(rn_geo_bearing_deg(0.0, 0.0, 1.0, 0.0), 0.0, 1e-6);
    CHECK_NEAR(rn_geo_bearing_deg(0.0, 0.0, 0.0, 1.0), 90.0, 1e-6);
    double x = 0.0;
    double y = 0.0;
    rn_geo_world_px(17.4455, 78.3489, 16u, &x, &y);
    double la = 0.0;
    double lo = 0.0;
    rn_geo_from_world_px(x, y, 16u, &la, &lo);
    CHECK_NEAR(la, 17.4455, 1e-9);
    CHECK_NEAR(lo, 78.3489, 1e-9);
    CHECK_NEAR(rn_geo_angle_diff_deg(350.0f, 10.0f), 20.0f, 1e-4f);
}

static void test_text(void)
{
    char road[41];
    rn_text_road_from_instruction(road, sizeof road, "Turn left onto Old Mumbai Hwy");
    CHECK(strcmp(road, "OLD MUMBAI HWY") == 0);
    rn_text_road_from_instruction(road, sizeof road, "Head north on Gachibowli Rd toward Road No. 2");
    CHECK(strcmp(road, "GACHIBOWLI RD") == 0);
    rn_text_road_from_instruction(road, sizeof road, "Head north on MG Road towards Main St");   /* en-IN */
    CHECK(strcmp(road, "MG ROAD") == 0);
    rn_text_road_from_instruction(road, sizeof road, "Turn left onto Brigade Rd\nDestination will be on the right");
    CHECK(strcmp(road, "BRIGADE RD") == 0);
    rn_text_road_from_instruction(road, sizeof road, "Turn left onto Toward Lane");
    CHECK(strcmp(road, "TOWARD LANE") == 0);
    rn_text_road_from_instruction(road, sizeof road, "Continue straight");
    CHECK(road[0] == '\0');
    rn_text_ascii_upper(road, 8u, "  ab\xc3\xa9  cd efgh");
    CHECK(strcmp(road, "AB CD E") == 0);
    CHECK(rn_text_contains_ci("Biodiversity Flyover", "FLYOVER"));
}

static void test_sun(void)
{
    /* Hyderabad, 2026-10-10: noon IST = 06:30 UTC, midnight IST = 18:30 UTC */
    double noon = 1791613800.0;
    CHECK(rn_sun_elevation_deg(17.44, 78.35, noon) > 55.0);
    CHECK(rn_sun_is_night(17.44, 78.35, noon + 12.0 * 3600.0));
    double ss = rn_sun_next_sunset(17.44, 78.35, noon);
    double local_h = (ss - noon) / 3600.0 + 12.0;             /* hours IST */
    CHECK(local_h > 17.7 && local_h < 18.3);                  /* ~17:57 */
}

void test_geo(void)
{
    (void)printf("geo\n");
    test_polyline();
    test_distances();
    test_text();
    test_sun();
}
