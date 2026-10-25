/*
 * rn_mapgen_thin.c - removes "roads" that are not part of a rideable road
 * network: footways and label halos (narrower than 3 px) and small
 * disconnected blobs (footpath dashes, icons). Removed pixels take the area
 * class around them.
 *
 *   1. morphological opening of the road mask with a 3x3 square
 *   2. 8-connected components of what survives; drop those < 60 px
 *   3. road pixels not kept -> unknown -> inpaint with area classes only
 */
#include "rn/rn_mapgen.h"

#include <string.h>

static bool is_road(uint8_t c)
{
    return c == (uint8_t)RN_CLASS_ROAD_MINOR || c == (uint8_t)RN_CLASS_ROAD_MAJOR;
}

/* dst = 1 where every 3x3 neighbour of src is 1 (outside counts as 0). */
static void erode3(const uint8_t *src, uint8_t *dst, uint32_t w, uint32_t h)
{
    for (uint32_t y = 0u; y < h; y++) {
        for (uint32_t x = 0u; x < w; x++) {
            uint8_t all = 1u;
            for (int32_t dy = -1; dy <= 1 && all != 0u; dy++) {
                for (int32_t dx = -1; dx <= 1; dx++) {
                    int32_t nx = (int32_t)x + dx;
                    int32_t ny = (int32_t)y + dy;
                    bool in = nx >= 0 && ny >= 0 && nx < (int32_t)w && ny < (int32_t)h;
                    if (!in || src[(uint32_t)ny * w + (uint32_t)nx] == 0u) {
                        all = 0u;
                    }
                }
            }
            dst[y * w + x] = all;
        }
    }
}

/* dst = 1 where any 3x3 neighbour of src is 1. */
static void dilate3(const uint8_t *src, uint8_t *dst, uint32_t w, uint32_t h)
{
    for (uint32_t y = 0u; y < h; y++) {
        for (uint32_t x = 0u; x < w; x++) {
            uint8_t any = 0u;
            for (int32_t dy = -1; dy <= 1 && any == 0u; dy++) {
                for (int32_t dx = -1; dx <= 1; dx++) {
                    int32_t nx = (int32_t)x + dx;
                    int32_t ny = (int32_t)y + dy;
                    bool in = nx >= 0 && ny >= 0 && nx < (int32_t)w && ny < (int32_t)h;
                    if (in && src[(uint32_t)ny * w + (uint32_t)nx] != 0u) {
                        any = 1u;
                    }
                }
            }
            dst[y * w + x] = any;
        }
    }
}

/* Breadth-first fill of the component containing `seed` in mask `keep`,
 * marking `seen`. Pixels are collected in list[0..n). Returns n. */
static uint32_t collect_component(const uint8_t *keep, uint8_t *seen, uint32_t *list,
                                  uint32_t w, uint32_t h, uint32_t seed)
{
    uint32_t head = 0u;
    uint32_t tail = 0u;
    uint32_t cap = w * h;
    list[tail] = seed;
    tail++;
    seen[seed] = 1u;
    while (head < tail && head < cap) {
        uint32_t i = list[head];
        head++;
        int32_t x = (int32_t)(i % w);
        int32_t y = (int32_t)(i / w);
        for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
                int32_t nx = x + dx;
                int32_t ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= (int32_t)w || ny >= (int32_t)h) {
                    continue;
                }
                uint32_t j = (uint32_t)ny * w + (uint32_t)nx;
                if (keep[j] != 0u && seen[j] == 0u && tail < cap) {
                    seen[j] = 1u;
                    list[tail] = j;
                    tail++;
                }
            }
        }
    }
    return tail;
}

/* Clear components smaller than RN_MAPGEN_MIN_ROAD_PX from keep. */
static void drop_small_blobs(uint8_t *keep, uint8_t *seen, uint32_t *list, uint32_t w, uint32_t h)
{
    uint32_t n = w * h;
    (void)memset(seen, 0, n);
    for (uint32_t i = 0u; i < n; i++) {
        if (keep[i] == 0u || seen[i] != 0u) {
            continue;
        }
        uint32_t size = collect_component(keep, seen, list, w, h, i);
        if (size < RN_MAPGEN_MIN_ROAD_PX) {
            for (uint32_t k = 0u; k < size; k++) {
                keep[list[k]] = 0u;
            }
        }
    }
}

void rn_mapgen_thin_roads(RnPlane *a, RnMapgenWork *work)
{
    if (!RN_ASSERT(a != NULL && a->px != NULL && work != NULL) ||
        !RN_ASSERT(a->w * a->h <= RN_MAPGEN_OUT_PIXELS)) {
        return;
    }
    uint32_t n = a->w * a->h;
    for (uint32_t i = 0u; i < n; i++) {
        work->m1[i] = is_road(a->px[i]) ? 1u : 0u;
    }
    erode3(work->m1, work->m2, a->w, a->h);
    dilate3(work->m2, work->b, a->w, a->h);          /* b = opened road mask */
    drop_small_blobs(work->b, work->m2, work->list, a->w, a->h);
    for (uint32_t i = 0u; i < n; i++) {
        if (work->m1[i] != 0u && work->b[i] == 0u) {
            a->px[i] = (uint8_t)RN_MAPGEN_UNKNOWN;
        }
    }
    rn_mapgen_inpaint(a, work->b, 30u, (uint8_t)RN_MAPGEN_AREA_CLASSES);
}
