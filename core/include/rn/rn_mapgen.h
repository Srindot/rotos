/*
 * rn_mapgen.h - turns OpenStreetMap raster tiles into clean, flat map-class
 * pixels (the "Pokemon overworld" look).
 *
 * OSM tiles are styled for big screens: anti-aliasing, labels, POI icons,
 * dashed footpaths. ROTOS wants a handful of flat classes that a 16-colour
 * theme can recolour. The pipeline works on one block at a time (bounded
 * memory, any route length):
 *
 *   rgb (zoom z+1) -> classify -> 2x majority downsample (zoom z)
 *     -> inpaint unknown -> harmonize roads -> thin roads out (OSM only)
 *     -> despeckle -> road outlines -> classes (zoom z)
 *
 * Callers feed blocks with a halo (RN_MAPGEN_HALO px at zoom z on every
 * side) and keep only the interior, so filters see real neighbours across
 * block edges.
 */
#ifndef RN_MAPGEN_H
#define RN_MAPGEN_H

#include "rn/rn_mappack.h"
#include "rn/rn_types.h"

#define RN_MAPGEN_UNKNOWN     255u
#define RN_MAPGEN_BLOCK       256u                    /* interior, zoom z  */
#define RN_MAPGEN_HALO        16u
#define RN_MAPGEN_MAX_OUT     (RN_MAPGEN_BLOCK + 2u * RN_MAPGEN_HALO)   /* 288 */
#define RN_MAPGEN_MAX_IN      (2u * RN_MAPGEN_MAX_OUT)                  /* 576 */
#define RN_MAPGEN_OUT_PIXELS  (RN_MAPGEN_MAX_OUT * RN_MAPGEN_MAX_OUT)
#define RN_MAPGEN_IN_PIXELS   (RN_MAPGEN_MAX_IN * RN_MAPGEN_MAX_IN)
#define RN_MAPGEN_COLOR_CACHE 4096u
#define RN_MAPGEN_MIN_ROAD_PX 60u       /* smaller road blobs are footpath dashes */
/* Tile style being converted. OSM carto has footpaths and label halos that
 * the road thinning removes; ROTOS's Google style has neither, so there
 * thinning would only delete real narrow lanes and is skipped. */
#define RN_MAPGEN_SRC_OSM     0u
#define RN_MAPGEN_SRC_GOOGLE  1u

typedef struct {
    uint8_t hi[RN_MAPGEN_IN_PIXELS];       /* classes at zoom z+1          */
    uint8_t a[RN_MAPGEN_OUT_PIXELS];       /* working plane                */
    uint8_t b[RN_MAPGEN_OUT_PIXELS];       /* scratch plane                */
    uint8_t m1[RN_MAPGEN_OUT_PIXELS];      /* masks                        */
    uint8_t m2[RN_MAPGEN_OUT_PIXELS];
    uint32_t list[RN_MAPGEN_OUT_PIXELS];   /* flood-fill queue             */
    uint32_t cache_key[RN_MAPGEN_COLOR_CACHE];
    uint8_t cache_val[RN_MAPGEN_COLOR_CACHE];
    uint32_t cache_magic;                  /* == RN_MAPGEN_CACHE_MAGIC once reset */
    uint8_t source;                        /* RN_MAPGEN_SRC_*              */
} RnMapgenWork;

#define RN_MAPGEN_CACHE_MAGIC 0x524D4743u

/* A plane of classes, w x h, row-major. */
typedef struct {
    uint8_t *px;
    uint32_t w;
    uint32_t h;
} RnPlane;

/* Full pipeline. rgb: w_in x h_in x 3 bytes at zoom z+1 (even sizes,
 * <= RN_MAPGEN_MAX_IN). out: (w_in/2) x (h_in/2) classes at zoom z. */
rn_status rn_mapgen_block(const uint8_t *rgb, uint32_t w_in, uint32_t h_in,
                          uint8_t *out, RnMapgenWork *work);

/* ---- stages (exposed for tests) ----------------------------------------- */

/* Bit set of classes (bit c = class c) for votes. */
#define RN_MAPGEN_ALL_CLASSES  0xFFu
#define RN_MAPGEN_AREA_CLASSES 0x1Fu    /* land, landuse, park, water, building */

void rn_mapgen_reset_cache(RnMapgenWork *work);
uint8_t rn_mapgen_classify_rgb(RnMapgenWork *work, uint8_t r, uint8_t g, uint8_t b);
void rn_mapgen_classify(const uint8_t *rgb, uint32_t n_pixels, uint8_t *out, RnMapgenWork *work);

/* Class with the most votes; ties go to the higher-priority class (roads
 * first, so 1 px roads stay connected). Returns RN_MAPGEN_UNKNOWN when no
 * allowed class has a vote. counts[8]. */
uint8_t rn_mapgen_vote(const uint8_t *counts, uint8_t allowed);
/* counts[8] of the 8 (or 4) neighbours of (x, y); outside = unknown. */
void rn_mapgen_neighbours(const RnPlane *p, uint32_t x, uint32_t y, bool diag, uint8_t *counts);

void rn_mapgen_downsample2(const uint8_t *hi, uint32_t w_in, uint32_t h_in, RnPlane *out);
void rn_mapgen_inpaint(RnPlane *a, uint8_t *scratch, uint32_t iterations, uint8_t allowed);
void rn_mapgen_harmonize_roads(RnPlane *a, uint8_t *scratch, uint32_t passes);
void rn_mapgen_thin_roads(RnPlane *a, RnMapgenWork *work);
void rn_mapgen_despeckle(RnPlane *a, uint8_t *scratch, uint32_t passes);
void rn_mapgen_outline_roads(RnPlane *a, uint8_t *scratch);

#endif /* RN_MAPGEN_H */
