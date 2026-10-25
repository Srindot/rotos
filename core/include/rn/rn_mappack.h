/*
 * rn_mappack.h - the ROTOS map pack (.rmap, format version 2): everything
 * the device needs to draw one route, built by the phone, sent over BLE,
 * stored in flash and read in place (memory-mapped, never copied to RAM).
 *
 *   offset  size  field
 *   0       4     magic "RMAP"
 *   4       2     version (2)
 *   6       1     zoom (Web Mercator zoom of one pack pixel)
 *   7       1     tile size (32)
 *   8       4     i32 origin_x   world px of the tile grid's top-left corner
 *   12      4     i32 origin_y
 *   16      2     tiles_w
 *   18      2     tiles_h
 *   20      2     n_slots        stored tiles
 *   22      2     n_maneuvers
 *   24      4     n_points
 *   28      4     f32 metres per pack pixel (at the route's latitude)
 *   32      4     f32 total route length, m
 *   36      4     u32 route duration estimate, s
 *   40      4     u32 route_id
 *   44      4     u32 offset of the index      u16[tiles_w * tiles_h], slot or 0xFFFF
 *   48      4     u32 offset of slot offsets   u32[n_slots], into the tile data
 *   52      4     u32 offset of tile data      RLE tiles (see rn_rle.h)
 *   56      4     u32 offset of points         n_points * (f32 x, f32 y, f32 along)
 *   60      4     u32 offset of maneuvers      n_maneuvers * 48-byte records
 *   64      4     u32 total length
 *   68      4     reserved (0)
 *
 *   maneuver record: u8 type, u8 hint, u16 point_idx, f32 along, char road[40]
 *   (NUL-padded, upper-case ASCII)
 *
 * Point coordinates are pack pixels relative to (origin_x, origin_y).
 */
#ifndef RN_MAPPACK_H
#define RN_MAPPACK_H

#include "rn/rn_maneuver.h"
#include "rn/rn_rle.h"
#include "rn/rn_types.h"

#define RN_PACK_MAGIC        "RMAP"
#define RN_PACK_VERSION      2u
#define RN_PACK_HEADER       72u
#define RN_PACK_MAN_RECORD   48u
#define RN_PACK_POINT_RECORD 12u
#define RN_PACK_NO_TILE      0xFFFFu
#define RN_PACK_MAX_SLOTS    65534u

/* Map classes: palette indices 0..7 (8..15 are UI colours on the device). */
#define RN_CLASS_LAND        0u
#define RN_CLASS_LANDUSE     1u
#define RN_CLASS_PARK        2u
#define RN_CLASS_WATER       3u
#define RN_CLASS_BUILDING    4u
#define RN_CLASS_ROAD_EDGE   5u
#define RN_CLASS_ROAD_MINOR  6u
#define RN_CLASS_ROAD_MAJOR  7u
#define RN_CLASS_COUNT       8u

typedef struct {
    uint8_t zoom;
    int32_t origin_x;
    int32_t origin_y;
    uint16_t tiles_w;
    uint16_t tiles_h;
    uint16_t n_slots;
    uint16_t n_maneuvers;
    uint32_t n_points;
    float m_per_px;
    float total_m;
    uint32_t duration_s;
    uint32_t route_id;
    uint32_t off_index;
    uint32_t off_slots;
    uint32_t off_tiles;
    uint32_t off_points;
    uint32_t off_mans;
    uint32_t total_len;
} RnPackHeader;

typedef struct {
    const uint8_t *blob;
    uint32_t len;
    RnPackHeader h;
} RnPack;

typedef struct {
    float x;      /* pack px */
    float y;
    float along;  /* metres  */
} RnPackPoint;

/* Validate and open a pack in place. All later accessors trust the
 * offsets checked here. */
rn_status rn_pack_open(RnPack *p, const uint8_t *blob, uint32_t len);

RnPackPoint rn_pack_point(const RnPack *p, uint32_t i);
rn_status rn_pack_maneuver(const RnPack *p, uint32_t i, RnManeuver *out);
float rn_pack_maneuver_along(const RnPack *p, uint32_t i);
uint8_t rn_pack_maneuver_type(const RnPack *p, uint32_t i);

/* Position on the route `along` metres from the start (pack px). */
RnPackPoint rn_pack_position_at(const RnPack *p, float along);

/* Slot of grid cell (cx, cy), or RN_PACK_NO_TILE. */
uint16_t rn_pack_slot(const RnPack *p, int32_t cx, int32_t cy);

/* ---- tile cache ---------------------------------------------------------
 * Decoded tiles (4 bpp, 512 B each) in a 4-way set-associative cache:
 * RN_TILE_CACHE_SETS * 4 tiles. 32 sets = 128 tiles = 64 KiB, enough for
 * every tile visible in a rotated 240x320 view.
 */
#ifndef RN_TILE_CACHE_SETS
#define RN_TILE_CACHE_SETS 32u
#endif
#define RN_TILE_CACHE_WAYS 4u

typedef struct {
    uint8_t px[RN_TILE_CACHE_SETS][RN_TILE_CACHE_WAYS][RN_TILE_PACKED];
    int32_t tag[RN_TILE_CACHE_SETS][RN_TILE_CACHE_WAYS];   /* slot, -1 = empty */
    uint8_t victim[RN_TILE_CACHE_SETS];
    uint32_t route_id;
    uint32_t hits;
    uint32_t misses;
    uint32_t errors;
} RnTileCache;

void rn_tile_cache_init(RnTileCache *c);
/* Packed 4 bpp pixels of a slot (decoded on a miss), or NULL on error. */
const uint8_t *rn_tile_cache_get(RnTileCache *c, const RnPack *p, uint16_t slot);

/* Map class at pack pixel (x, y), or -1 outside the stored corridor. */
int32_t rn_pack_class_at(const RnPack *p, RnTileCache *c, int32_t x, int32_t y);

#endif /* RN_MAPPACK_H */
