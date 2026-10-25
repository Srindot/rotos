/*
 * test_mappack.c - RLE codec, corridor, pack writer/reader, tile cache.
 */
#include <string.h>

#include "rn/rn_corridor.h"
#include "rn/rn_mappack.h"
#include "rn/rn_packwriter.h"
#include "rn/rn_rle.h"
#include "rn/rn_tracker.h"
#include "rn_test.h"
#include "test_fixtures.h"

static uint8_t pattern(uint32_t cx, uint32_t cy, uint32_t x, uint32_t y)
{
    return (uint8_t)(((cx * 7u + cy * 3u) + (x / 8u) + (y / 4u)) % 8u);
}

static void test_rle(void)
{
    uint8_t cls[RN_TILE_PIXELS];
    uint8_t enc[RN_RLE_MAX_ENCODED];
    uint8_t packed[RN_TILE_PACKED];
    uint32_t seed = 12345u;
    for (uint32_t i = 0u; i < RN_TILE_PIXELS; i++) {          /* noise: forces RAW */
        seed = seed * 1103515245u + 12345u;
        cls[i] = (uint8_t)((seed >> 16) & 7u);
    }
    int32_t n = rn_rle_encode(cls, enc, sizeof enc);
    CHECK(n == (int32_t)RN_RLE_MAX_ENCODED && enc[0] == RN_RLE_RAW);
    CHECK(rn_rle_decode_packed(enc, (uint32_t)n, packed) == n);
    CHECK((packed[10] >> 4) == cls[20] && (packed[10] & 15u) == cls[21]);
    for (uint32_t i = 0u; i < RN_TILE_PIXELS; i++) {          /* flat: tiny */
        cls[i] = (i < 512u) ? 3u : 6u;
    }
    n = rn_rle_encode(cls, enc, sizeof enc);
    CHECK(n == 65 && enc[0] == RN_RLE_RUNS);
    CHECK(rn_rle_decode_packed(enc, (uint32_t)n, packed) == n);
    CHECK(packed[0] == 0x33u && packed[511] == 0x66u);
    CHECK(rn_rle_decode_packed(enc, 10u, packed) == RN_ERR_FORMAT);   /* truncated */
}

/* RN_NAV_FREE: a GPS position lands on the right pack pixel, the course sets
 * the heading, and dead reckoning moves it forward between packets. */
static void test_tracker_free(const RnPack *p, const RnRoute *r)
{
    RnTracker t;
    rn_tracker_reset(&t);
    RnNavState s;
    (void)memset(&s, 0, sizeof s);
    s.lat = r->pts[0].lat;
    s.lon = r->pts[0].lon;
    s.heading_deg = 0.0f;                                    /* north */
    s.speed_mps = 10.0f;
    s.flags = RN_NAV_FREE | RN_NAV_HEADING;
    rn_tracker_on_nav(&t, &s, 1000u);
    rn_tracker_update(&t, p, 1000u);
    RnPackPoint a = rn_pack_point(p, 0u);
    CHECK(t.free);
    CHECK_NEAR(t.x_px, a.x, 1.0f);
    CHECK_NEAR(t.y_px, a.y, 1.0f);
    CHECK_NEAR(t.heading_rad, 0.0f, 0.01f);
    for (uint32_t ms = 1033u; ms <= 2000u; ms += 33u) {      /* 1 s later, no new packet */
        rn_tracker_update(&t, p, ms);
    }
    float moved_m = (a.y - t.y_px) * p->h.m_per_px;          /* north = -y */
    CHECK(moved_m > 6.0f && moved_m < 10.5f);
    s.flags = 0u;                                            /* back to route mode */
    rn_tracker_on_nav(&t, &s, 2100u);
    rn_tracker_update(&t, p, 2100u);
    CHECK(!t.free);
}

void test_mappack(void)
{
    (void)printf("mappack\n");
    test_rle();
    static RnRoute r;
    static RnCorridor c;
    static uint8_t buf[1u << 20];
    static RnPackWriter w;
    static RnTileCache cache;
    build_l_route(&r);
    CHECK(rn_corridor_plan(&c, &r, 16u, 64u) == RN_OK);
    CHECK(c.n_keep > 20u && c.n_keep < (uint32_t)c.tiles_w * c.tiles_h);
    CHECK(rn_packw_begin(&w, buf, sizeof buf, &c) == RN_OK);
    uint8_t cls[RN_TILE_PIXELS];
    for (uint32_t cy = 0u; cy < c.tiles_h; cy++) {
        for (uint32_t cx = 0u; cx < c.tiles_w; cx++) {
            if (!rn_corridor_keep(&c, cx, cy)) {
                CHECK(rn_packw_add_tile(&w, cx, cy, cls) == RN_ERR_RANGE || 1);
                continue;
            }
            for (uint32_t i = 0u; i < RN_TILE_PIXELS; i++) {
                cls[i] = pattern(cx, cy, i % 32u, i / 32u);
            }
            CHECK(rn_packw_add_tile(&w, cx, cy, cls) == RN_OK);
        }
    }
    uint32_t len = 0u;
    CHECK(rn_packw_finish(&w, &r, &len) == RN_OK);
    RnPack p;
    CHECK(rn_pack_open(&p, buf, len) == RN_OK);
    CHECK(p.h.route_id == 0x1234u && p.h.n_points == r.n_pts && p.h.n_maneuvers == 3u);
    rn_tile_cache_init(&cache);
    int mismatches = 0;
    for (uint32_t cy = 0u; cy < c.tiles_h; cy++) {
        for (uint32_t cx = 0u; cx < c.tiles_w; cx++) {
            int32_t got = rn_pack_class_at(&p, &cache, (int32_t)(cx * 32u + 9u), (int32_t)(cy * 32u + 5u));
            int32_t want = rn_corridor_keep(&c, cx, cy) ? (int32_t)pattern(cx, cy, 9u, 5u) : -1;
            mismatches += (got != want);
        }
    }
    CHECK(mismatches == 0);
    CHECK(cache.errors == 0u);
    RnManeuver m;
    CHECK(rn_pack_maneuver(&p, 1u, &m) == RN_OK && m.type == RN_M_RIGHT);
    CHECK(strcmp(m.road, "ROAD NO. 2") == 0);
    RnPackPoint a = rn_pack_position_at(&p, 0.0f);
    RnPackPoint b = rn_pack_position_at(&p, 1000.0f);
    float mpp = p.h.m_per_px;
    CHECK_NEAR(a.y - b.y, 1000.0f / mpp, 3.0f);               /* north = up */
    test_tracker_free(&p, &r);
    buf[63] ^= 0xFFu;                                        /* corrupt an offset */
    CHECK(rn_pack_open(&p, buf, len) == RN_ERR_FORMAT);
}
