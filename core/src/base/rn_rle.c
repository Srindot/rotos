/*
 * rn_rle.c - map tile codec.
 */
#include "rn/rn_rle.h"

#include <string.h>

/* Count runs; returns the encoded size of the RUNS form (incl. mode byte). */
static uint32_t runs_size(const uint8_t *classes)
{
    uint32_t n = 1u;
    uint32_t i = 0u;
    while (i < RN_TILE_PIXELS) {
        uint32_t run = 1u;
        while (run < 16u && i + run < RN_TILE_PIXELS && classes[i + run] == classes[i]) {
            run++;
        }
        n++;
        i += run;
    }
    return n;
}

static void encode_raw(const uint8_t *classes, uint8_t *out)
{
    out[0] = (uint8_t)RN_RLE_RAW;
    for (uint32_t i = 0u; i < RN_TILE_PACKED; i++) {
        out[1u + i] = (uint8_t)(((classes[2u * i] & 0x0Fu) << 4) | (classes[2u * i + 1u] & 0x0Fu));
    }
}

static uint32_t encode_runs(const uint8_t *classes, uint8_t *out)
{
    uint32_t n = 1u;
    uint32_t i = 0u;
    out[0] = (uint8_t)RN_RLE_RUNS;
    while (i < RN_TILE_PIXELS) {
        uint32_t run = 1u;
        while (run < 16u && i + run < RN_TILE_PIXELS && classes[i + run] == classes[i]) {
            run++;
        }
        out[n] = (uint8_t)(((run - 1u) << 4) | (classes[i] & 0x0Fu));
        n++;
        i += run;
    }
    return n;
}

int32_t rn_rle_encode(const uint8_t *classes, uint8_t *out, uint32_t cap)
{
    if (!RN_ASSERT(classes != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    for (uint32_t i = 0u; i < RN_TILE_PIXELS; i++) {
        if (!RN_ASSERT(classes[i] < 16u)) {
            return RN_ERR_RANGE;
        }
    }
    uint32_t rs = runs_size(classes);
    if (rs < RN_RLE_MAX_ENCODED) {
        if (cap < rs) {
            return RN_ERR_FULL;
        }
        return (int32_t)encode_runs(classes, out);
    }
    if (cap < RN_RLE_MAX_ENCODED) {
        return RN_ERR_FULL;
    }
    encode_raw(classes, out);
    return (int32_t)RN_RLE_MAX_ENCODED;
}

int32_t rn_rle_decode_packed(const uint8_t *src, uint32_t avail, uint8_t *packed)
{
    if (!RN_ASSERT(src != NULL && packed != NULL) || avail < 1u) {
        return RN_ERR_PARAM;
    }
    if (src[0] == (uint8_t)RN_RLE_RAW) {
        if (avail < RN_RLE_MAX_ENCODED) {
            return RN_ERR_FORMAT;
        }
        (void)memcpy(packed, &src[1], RN_TILE_PACKED);
        return (int32_t)RN_RLE_MAX_ENCODED;
    }
    if (src[0] != (uint8_t)RN_RLE_RUNS) {
        return RN_ERR_FORMAT;
    }
    uint32_t px = 0u;
    uint32_t pos = 1u;
    for (uint32_t guard = 0u; guard < RN_TILE_PIXELS && px < RN_TILE_PIXELS; guard++) {
        if (pos >= avail) {
            return RN_ERR_FORMAT;
        }
        uint32_t run = ((uint32_t)src[pos] >> 4) + 1u;
        uint8_t c = (uint8_t)(src[pos] & 0x0Fu);
        pos++;
        if (px + run > RN_TILE_PIXELS) {
            return RN_ERR_FORMAT;
        }
        for (uint32_t k = 0u; k < run; k++) {
            uint32_t p = px + k;
            uint8_t *b = &packed[p >> 1];
            *b = ((p & 1u) != 0u) ? (uint8_t)((*b & 0xF0u) | c) : (uint8_t)((*b & 0x0Fu) | (uint8_t)(c << 4));
        }
        px += run;
    }
    return (px == RN_TILE_PIXELS) ? (int32_t)pos : RN_ERR_FORMAT;
}
