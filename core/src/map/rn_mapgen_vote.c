/*
 * rn_mapgen_vote.c - neighbourhood counting and the priority vote that every
 * filter stage uses.
 */
#include "rn/rn_mapgen.h"

#include <string.h>

/* On a tie the earlier class wins: keeps 1 px roads connected. */
static const uint8_t PRIORITY[RN_CLASS_COUNT] = {
    RN_CLASS_ROAD_MAJOR, RN_CLASS_ROAD_MINOR, RN_CLASS_WATER, RN_CLASS_BUILDING,
    RN_CLASS_PARK, RN_CLASS_LANDUSE, RN_CLASS_LAND, RN_CLASS_ROAD_EDGE,
};

static const int8_t DX8[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const int8_t DY8[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
static const int8_t DX4[4] = {0, -1, 1, 0};
static const int8_t DY4[4] = {-1, 0, 0, 1};

uint8_t rn_mapgen_vote(const uint8_t *counts, uint8_t allowed)
{
    if (!RN_ASSERT(counts != NULL)) {
        return (uint8_t)RN_MAPGEN_UNKNOWN;
    }
    uint8_t best = (uint8_t)RN_MAPGEN_UNKNOWN;
    uint8_t best_n = 0u;
    for (uint32_t k = 0u; k < RN_CLASS_COUNT; k++) {
        uint8_t c = PRIORITY[k];
        bool ok = (allowed & (uint8_t)(1u << c)) != 0u;
        if (ok && counts[c] > best_n) {
            best_n = counts[c];
            best = c;
        }
    }
    return best;
}

void rn_mapgen_neighbours(const RnPlane *p, uint32_t x, uint32_t y, bool diag, uint8_t *counts)
{
    if (!RN_ASSERT(p != NULL && counts != NULL) || !RN_ASSERT(x < p->w && y < p->h)) {
        return;
    }
    (void)memset(counts, 0, RN_CLASS_COUNT);
    uint32_t n = diag ? 8u : 4u;
    for (uint32_t k = 0u; k < n; k++) {
        int32_t nx = (int32_t)x + (diag ? DX8[k] : DX4[k]);
        int32_t ny = (int32_t)y + (diag ? DY8[k] : DY4[k]);
        if (nx < 0 || ny < 0 || nx >= (int32_t)p->w || ny >= (int32_t)p->h) {
            continue;
        }
        uint8_t c = p->px[(uint32_t)ny * p->w + (uint32_t)nx];
        if (c < RN_CLASS_COUNT) {
            counts[c]++;
        }
    }
}
