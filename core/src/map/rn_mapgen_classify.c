/*
 * rn_mapgen_classify.c - OSM "carto" colours -> map classes.
 *
 * Every colour is matched to the nearest reference colour; anything further
 * than 22 RGB units away (text, icons, anti-aliased edges) is UNKNOWN and is
 * filled in from its neighbours later.
 */
#include "rn/rn_mapgen.h"

#define MAX_DIST2 (22 * 22)
#define EMPTY_KEY 0xFFFFFFFFu

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t cls;
} RefColor;

static const RefColor REFERENCE[] = {
    {242u, 239u, 233u, RN_CLASS_LAND},        /* background                 */
    {238u, 240u, 213u, RN_CLASS_LAND},        /* farmland                   */
    {224u, 223u, 223u, RN_CLASS_LANDUSE},     /* residential                */
    {242u, 218u, 217u, RN_CLASS_LANDUSE},     /* commercial                 */
    {255u, 214u, 209u, RN_CLASS_LANDUSE},     /* retail                     */
    {235u, 219u, 232u, RN_CLASS_LANDUSE},     /* industrial / railway       */
    {199u, 199u, 180u, RN_CLASS_LANDUSE},     /* construction / brownfield  */
    {223u, 221u, 206u, RN_CLASS_LANDUSE},     /* garages                    */
    {238u, 238u, 238u, RN_CLASS_LANDUSE},     /* parking                    */
    {221u, 221u, 232u, RN_CLASS_LANDUSE},     /* pedestrian area            */
    {255u, 255u, 229u, RN_CLASS_LANDUSE},     /* education / hospital       */
    {246u, 238u, 183u, RN_CLASS_LANDUSE},     /* school / university        */
    {200u, 250u, 204u, RN_CLASS_PARK},        /* park                       */
    {205u, 235u, 176u, RN_CLASS_PARK},        /* grass / meadow             */
    {173u, 209u, 158u, RN_CLASS_PARK},        /* forest                     */
    {200u, 215u, 171u, RN_CLASS_PARK},        /* scrub                      */
    {170u, 224u, 203u, RN_CLASS_PARK},        /* pitch                      */
    {222u, 246u, 192u, RN_CLASS_PARK},        /* golf / village green       */
    {170u, 203u, 175u, RN_CLASS_PARK},        /* cemetery                   */
    {223u, 252u, 226u, RN_CLASS_PARK},        /* recreation ground          */
    {170u, 211u, 223u, RN_CLASS_WATER},
    {217u, 208u, 201u, RN_CLASS_BUILDING},
    {196u, 182u, 171u, RN_CLASS_BUILDING},    /* building outline           */
    {255u, 255u, 255u, RN_CLASS_ROAD_MINOR},  /* tertiary / residential     */
    {247u, 250u, 191u, RN_CLASS_ROAD_MAJOR},  /* secondary                  */
    {252u, 214u, 164u, RN_CLASS_ROAD_MAJOR},  /* primary                    */
    {249u, 178u, 156u, RN_CLASS_ROAD_MAJOR},  /* trunk                      */
    {232u, 146u, 162u, RN_CLASS_ROAD_MAJOR},  /* motorway                   */
};

#define N_REF ((uint32_t)(sizeof REFERENCE / sizeof REFERENCE[0]))

void rn_mapgen_reset_cache(RnMapgenWork *work)
{
    if (!RN_ASSERT(work != NULL)) {
        return;
    }
    for (uint32_t i = 0u; i < RN_MAPGEN_COLOR_CACHE; i++) {
        work->cache_key[i] = EMPTY_KEY;
        work->cache_val[i] = (uint8_t)RN_MAPGEN_UNKNOWN;
    }
    work->cache_magic = RN_MAPGEN_CACHE_MAGIC;
}

static uint8_t nearest(uint8_t r, uint8_t g, uint8_t b)
{
    int32_t best = MAX_DIST2 + 1;
    uint8_t cls = (uint8_t)RN_MAPGEN_UNKNOWN;
    for (uint32_t i = 0u; i < N_REF; i++) {
        int32_t dr = (int32_t)r - (int32_t)REFERENCE[i].r;
        int32_t dg = (int32_t)g - (int32_t)REFERENCE[i].g;
        int32_t db = (int32_t)b - (int32_t)REFERENCE[i].b;
        int32_t d2 = dr * dr + dg * dg + db * db;
        if (d2 < best) {
            best = d2;
            cls = REFERENCE[i].cls;
        }
    }
    return cls;
}

uint8_t rn_mapgen_classify_rgb(RnMapgenWork *work, uint8_t r, uint8_t g, uint8_t b)
{
    if (!RN_ASSERT(work != NULL)) {
        return (uint8_t)RN_MAPGEN_UNKNOWN;
    }
    if (work->cache_magic != RN_MAPGEN_CACHE_MAGIC) {
        rn_mapgen_reset_cache(work);
    }
    uint32_t key = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    uint32_t slot = ((key * 2654435761u) >> 20) % RN_MAPGEN_COLOR_CACHE;
    if (work->cache_key[slot] != key) {
        work->cache_key[slot] = key;
        work->cache_val[slot] = nearest(r, g, b);
    }
    return work->cache_val[slot];
}

void rn_mapgen_classify(const uint8_t *rgb, uint32_t n_pixels, uint8_t *out, RnMapgenWork *work)
{
    if (!RN_ASSERT(rgb != NULL && out != NULL && work != NULL) ||
        !RN_ASSERT(n_pixels <= RN_MAPGEN_IN_PIXELS)) {
        return;
    }
    for (uint32_t i = 0u; i < n_pixels; i++) {
        out[i] = rn_mapgen_classify_rgb(work, rgb[3u * i], rgb[3u * i + 1u], rgb[3u * i + 2u]);
    }
}
