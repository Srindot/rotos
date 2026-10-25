/*
 * rn_polyline.h - Google "encoded polyline" format (precision 1e5), as used
 * by the Routes API (routes[].legs[].steps[].polyline.encodedPolyline).
 */
#ifndef RN_POLYLINE_H
#define RN_POLYLINE_H

#include "rn/rn_types.h"

/* Decode up to cap points. Returns the number of points decoded, or a
 * negative rn_status (RN_ERR_FORMAT on a truncated string, RN_ERR_FULL when
 * more than cap points are encoded). */
int32_t rn_polyline_decode(const char *enc, double *lat, double *lon, uint32_t cap);

/* Encode n points; returns the string length or a negative rn_status. */
int32_t rn_polyline_encode(const double *lat, const double *lon, uint32_t n,
                           char *out, uint32_t cap);

#endif /* RN_POLYLINE_H */
