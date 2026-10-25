/*
 * rn_polyline.c - Google encoded polyline codec.
 */
#include "rn/rn_polyline.h"

#include <math.h>

#define MAX_CHUNKS 7u   /* a 32-bit delta needs at most 7 five-bit chunks */

/* Decode one signed varint starting at *pos. Returns false on bad input. */
static bool read_value(const char *enc, uint32_t len, uint32_t *pos, int32_t *out)
{
    uint32_t result = 0u;
    uint32_t shift = 0u;
    for (uint32_t k = 0u; k < MAX_CHUNKS; k++) {
        if (*pos >= len) {
            return false;
        }
        int32_t b = (int32_t)(unsigned char)enc[*pos] - 63;
        *pos += 1u;
        if (b < 0 || b > 63) {
            return false;
        }
        result |= ((uint32_t)b & 0x1Fu) << shift;
        shift += 5u;
        if (b < 0x20) {
            *out = ((result & 1u) != 0u) ? ~(int32_t)(result >> 1) : (int32_t)(result >> 1);
            return true;
        }
    }
    return false;
}

int32_t rn_polyline_decode(const char *enc, double *lat, double *lon, uint32_t cap)
{
    if (!RN_ASSERT(enc != NULL && lat != NULL && lon != NULL)) {
        return RN_ERR_PARAM;
    }
    uint32_t len = 0u;
    while (len < RN_MAX_POLYLINE_LEN && enc[len] != '\0') {
        len++;
    }
    uint32_t pos = 0u;
    uint32_t n = 0u;
    int32_t la = 0;
    int32_t lo = 0;
    for (uint32_t guard = 0u; guard < RN_MAX_POLYLINE_LEN && pos < len; guard++) {
        int32_t dla = 0;
        int32_t dlo = 0;
        if (!read_value(enc, len, &pos, &dla) || !read_value(enc, len, &pos, &dlo)) {
            return RN_ERR_FORMAT;
        }
        if (n >= cap) {
            return RN_ERR_FULL;
        }
        la += dla;
        lo += dlo;
        lat[n] = (double)la * 1e-5;
        lon[n] = (double)lo * 1e-5;
        n++;
    }
    return (int32_t)n;
}

/* Append one signed value; returns false when out of space. */
static bool write_value(int32_t v, char *out, uint32_t cap, uint32_t *pos)
{
    uint32_t u = (v < 0) ? ~((uint32_t)v << 1) : ((uint32_t)v << 1);
    for (uint32_t k = 0u; k < MAX_CHUNKS; k++) {
        if (*pos + 1u >= cap) {
            return false;
        }
        uint32_t chunk = u & 0x1Fu;
        u >>= 5;
        if (u != 0u) {
            chunk |= 0x20u;
        }
        out[*pos] = (char)(chunk + 63u);
        *pos += 1u;
        if (u == 0u) {
            return true;
        }
    }
    return false;
}

int32_t rn_polyline_encode(const double *lat, const double *lon, uint32_t n,
                           char *out, uint32_t cap)
{
    if (!RN_ASSERT(lat != NULL && lon != NULL && out != NULL) || !RN_ASSERT(cap > 0u)) {
        return RN_ERR_PARAM;
    }
    uint32_t pos = 0u;
    int32_t pla = 0;
    int32_t plo = 0;
    for (uint32_t i = 0u; i < n; i++) {
        int32_t la = (int32_t)lround(lat[i] * 1e5);
        int32_t lo = (int32_t)lround(lon[i] * 1e5);
        if (!write_value(la - pla, out, cap, &pos) || !write_value(lo - plo, out, cap, &pos)) {
            out[0] = '\0';
            return RN_ERR_FULL;
        }
        pla = la;
        plo = lo;
    }
    out[pos] = '\0';
    return (int32_t)pos;
}
