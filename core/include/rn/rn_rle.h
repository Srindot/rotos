/*
 * rn_rle.h - map tile codec.
 *
 * A tile is 32 x 32 map-class pixels (4 bits each). Stored form:
 *
 *   byte 0 = mode
 *     RN_RLE_RAW : 512 bytes follow, 4 bpp, high nibble = left pixel
 *     RN_RLE_RUNS: run bytes follow until 1024 pixels are covered;
 *                  each byte = (run_length - 1) << 4 | class, runs 1..16
 *
 * Map tiles are mostly flat areas and straight road edges, so runs shrink
 * them about 3x; the encoder falls back to RAW whenever runs would not win.
 * Decoding is a single bounded pass, cheap enough to do on the ESP32-C3
 * whenever a tile enters the tile cache.
 */
#ifndef RN_RLE_H
#define RN_RLE_H

#include "rn/rn_types.h"

#define RN_TILE              32u
#define RN_TILE_PIXELS       (RN_TILE * RN_TILE)
#define RN_TILE_PACKED       (RN_TILE_PIXELS / 2u)       /* 4 bpp bytes  */
#define RN_RLE_RAW           0u
#define RN_RLE_RUNS          1u
#define RN_RLE_MAX_ENCODED   (1u + RN_TILE_PACKED)       /* worst case   */

/* classes: 1024 bytes, one class (0..15) per pixel, row-major.
 * Returns encoded length (>= 1) or a negative rn_status. */
int32_t rn_rle_encode(const uint8_t *classes, uint8_t *out, uint32_t cap);

/* Decode into 512 packed bytes (4 bpp). avail = bytes available at src.
 * Returns bytes consumed or a negative rn_status. */
int32_t rn_rle_decode_packed(const uint8_t *src, uint32_t avail, uint8_t *packed);

#endif /* RN_RLE_H */
