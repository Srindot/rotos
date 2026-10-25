/*
 * rn_mapgen_filters.c - neighbourhood filters: 2x majority downsample,
 * inpainting, road harmonisation, despeckle and road outlines.
 *
 * Every filter is "Jacobi" style: it reads the plane as it was at the start
 * of the pass and writes into a scratch plane, so results never depend on
 * scan order.
 */
#include "rn/rn_mapgen.h"

#include <string.h>

#define ROAD_MAJOR_NEIGHBOURS 3u

static bool is_road(uint8_t c)
{
    return c == (uint8_t)RN_CLASS_ROAD_MINOR || c == (uint8_t)RN_CLASS_ROAD_MAJOR;
}

void rn_mapgen_downsample2(const uint8_t *hi, uint32_t w_in, uint32_t h_in, RnPlane *out)
{
    if (!RN_ASSERT(hi != NULL && out != NULL && out->px != NULL) ||
        !RN_ASSERT(w_in / 2u == out->w && h_in / 2u == out->h)) {
        return;
    }
    for (uint32_t y = 0u; y < out->h; y++) {
        for (uint32_t x = 0u; x < out->w; x++) {
            uint8_t counts[RN_CLASS_COUNT] = {0u};
            uint32_t base = (2u * y) * w_in + 2u * x;
            uint8_t q[4] = {hi[base], hi[base + 1u], hi[base + w_in], hi[base + w_in + 1u]};
            for (uint32_t k = 0u; k < 4u; k++) {
                if (q[k] < RN_CLASS_COUNT) {
                    counts[q[k]]++;
                }
            }
            out->px[y * out->w + x] = rn_mapgen_vote(counts, RN_MAPGEN_ALL_CLASSES);
        }
    }
}

/* One inpaint pass; returns the number of pixels still unknown. */
static uint32_t inpaint_pass(RnPlane *a, uint8_t *scratch, uint8_t allowed)
{
    uint32_t n = a->w * a->h;
    uint32_t left = 0u;
    (void)memcpy(scratch, a->px, n);
    for (uint32_t y = 0u; y < a->h; y++) {
        for (uint32_t x = 0u; x < a->w; x++) {
            uint32_t i = y * a->w + x;
            if (a->px[i] != (uint8_t)RN_MAPGEN_UNKNOWN) {
                continue;
            }
            uint8_t counts[RN_CLASS_COUNT];
            rn_mapgen_neighbours(a, x, y, true, counts);
            scratch[i] = rn_mapgen_vote(counts, allowed);
            left += (scratch[i] == (uint8_t)RN_MAPGEN_UNKNOWN) ? 1u : 0u;
        }
    }
    (void)memcpy(a->px, scratch, n);
    return left;
}

void rn_mapgen_inpaint(RnPlane *a, uint8_t *scratch, uint32_t iterations, uint8_t allowed)
{
    if (!RN_ASSERT(a != NULL && a->px != NULL && scratch != NULL) ||
        !RN_ASSERT(a->w * a->h <= RN_MAPGEN_OUT_PIXELS)) {
        return;
    }
    uint32_t n = a->w * a->h;
    uint32_t left = 1u;
    for (uint32_t it = 0u; it < iterations && it < 64u && left > 0u; it++) {
        left = inpaint_pass(a, scratch, allowed);
    }
    for (uint32_t i = 0u; i < n; i++) {
        if (a->px[i] == (uint8_t)RN_MAPGEN_UNKNOWN) {
            a->px[i] = (uint8_t)RN_CLASS_LAND;
        }
    }
}

void rn_mapgen_harmonize_roads(RnPlane *a, uint8_t *scratch, uint32_t passes)
{
    if (!RN_ASSERT(a != NULL && a->px != NULL && scratch != NULL) || !RN_ASSERT(passes <= 8u)) {
        return;
    }
    uint32_t n = a->w * a->h;
    for (uint32_t p = 0u; p < passes; p++) {
        (void)memcpy(scratch, a->px, n);
        for (uint32_t y = 0u; y < a->h; y++) {
            for (uint32_t x = 0u; x < a->w; x++) {
                uint32_t i = y * a->w + x;
                if (a->px[i] != (uint8_t)RN_CLASS_ROAD_MINOR) {
                    continue;
                }
                uint8_t counts[RN_CLASS_COUNT];
                rn_mapgen_neighbours(a, x, y, true, counts);
                if (counts[RN_CLASS_ROAD_MAJOR] >= ROAD_MAJOR_NEIGHBOURS) {
                    scratch[i] = (uint8_t)RN_CLASS_ROAD_MAJOR;   /* label halo inside a major road */
                }
            }
        }
        (void)memcpy(a->px, scratch, n);
    }
}

void rn_mapgen_despeckle(RnPlane *a, uint8_t *scratch, uint32_t passes)
{
    if (!RN_ASSERT(a != NULL && a->px != NULL && scratch != NULL) || !RN_ASSERT(passes <= 8u)) {
        return;
    }
    uint32_t n = a->w * a->h;
    for (uint32_t p = 0u; p < passes; p++) {
        (void)memcpy(scratch, a->px, n);
        for (uint32_t y = 0u; y < a->h; y++) {
            for (uint32_t x = 0u; x < a->w; x++) {
                uint32_t i = y * a->w + x;
                uint8_t counts[RN_CLASS_COUNT];
                rn_mapgen_neighbours(a, x, y, true, counts);
                uint8_t c = a->px[i];
                bool lonely = c < RN_CLASS_COUNT && counts[c] == 0u;
                uint8_t best = rn_mapgen_vote(counts, RN_MAPGEN_ALL_CLASSES);
                if (lonely && best != (uint8_t)RN_MAPGEN_UNKNOWN) {
                    scratch[i] = best;
                }
            }
        }
        (void)memcpy(a->px, scratch, n);
    }
}

void rn_mapgen_outline_roads(RnPlane *a, uint8_t *scratch)
{
    if (!RN_ASSERT(a != NULL && a->px != NULL && scratch != NULL) ||
        !RN_ASSERT(a->w * a->h <= RN_MAPGEN_OUT_PIXELS)) {
        return;
    }
    uint32_t n = a->w * a->h;
    (void)memcpy(scratch, a->px, n);
    for (uint32_t y = 0u; y < a->h; y++) {
        for (uint32_t x = 0u; x < a->w; x++) {
            uint32_t i = y * a->w + x;
            if (is_road(a->px[i])) {
                continue;
            }
            uint8_t counts[RN_CLASS_COUNT];
            rn_mapgen_neighbours(a, x, y, false, counts);
            if (counts[RN_CLASS_ROAD_MINOR] + counts[RN_CLASS_ROAD_MAJOR] > 0u) {
                scratch[i] = (uint8_t)RN_CLASS_ROAD_EDGE;
            }
        }
    }
    (void)memcpy(a->px, scratch, n);
}
