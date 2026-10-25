/*
 * test_mapgen.c - raster pipeline on a synthetic OSM-like image.
 */
#include <string.h>

#include "rn/rn_mapgen.h"
#include "rn_test.h"

#define W 128u
#define H 128u

static void put(uint8_t *rgb, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t *p = &rgb[(y * W + x) * 3u];
    p[0] = r;
    p[1] = g;
    p[2] = b;
}

void test_mapgen(void)
{
    (void)printf("mapgen\n");
    static uint8_t rgb[W * H * 3u];
    static uint8_t out[(W / 2u) * (H / 2u)];
    static RnMapgenWork work;
    for (uint32_t y = 0u; y < H; y++) {
        for (uint32_t x = 0u; x < W; x++) {
            put(rgb, x, y, 242u, 239u, 233u);                  /* land */
            if (y >= 60u && y < 72u) {
                put(rgb, x, y, 252u, 214u, 164u);              /* primary road, 12 px */
            }
            if (x >= 30u && x < 32u && y < 60u) {
                put(rgb, x, y, 255u, 255u, 255u);              /* 2 px footway */
            }
            if (x >= 90u && x < 100u && y >= 20u && y < 26u) {
                put(rgb, x, y, 20u, 20u, 20u);                 /* label text */
            }
        }
    }
    CHECK(rn_mapgen_block(rgb, W, H, out, &work) == RN_OK);
    const uint32_t ow = W / 2u;
    CHECK(out[33u * ow + 20u] == RN_CLASS_ROAD_MAJOR);          /* road centre    */
    CHECK(out[29u * ow + 20u] == RN_CLASS_ROAD_EDGE);           /* outline above  */
    CHECK(out[15u * ow + 15u] == RN_CLASS_LAND);                /* footway gone   */
    CHECK(out[11u * ow + 47u] == RN_CLASS_LAND);                /* text inpainted */
    uint32_t unknown = 0u;
    for (uint32_t i = 0u; i < ow * (H / 2u); i++) {
        unknown += out[i] >= RN_CLASS_COUNT;
    }
    CHECK(unknown == 0u);
    CHECK(rn_mapgen_block(rgb, 3u, 4u, out, &work) == RN_ERR_RANGE);
}
