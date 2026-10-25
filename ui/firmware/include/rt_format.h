/*
 * rt_format.h - small bounded text formatters (no printf, no floats in
 * format strings). Every function NUL-terminates and never writes past cap.
 */
#ifndef RT_FORMAT_H
#define RT_FORMAT_H

#include <stdint.h>

#define RT_VALUE_LEN 16u
#define RT_UNIT_LEN  8u

/* Append src to dst (cap bytes total). Returns the new length. */
uint32_t rt_fmt_cat(char *dst, uint32_t cap, const char *src);
/* Unsigned decimal, zero-padded to min_digits. */
uint32_t rt_fmt_u32(char *dst, uint32_t cap, uint32_t v, uint32_t min_digits);
/* Signed, rounded to the nearest integer. */
uint32_t rt_fmt_round(char *dst, uint32_t cap, float v);
/* "240" + "M", "1.2" + "KM", "12" + "KM" (value: 16 bytes, unit: 8 bytes). */
void rt_fmt_distance(float m, char *value, char *unit);
/* "HH:MM" from seconds of the day. */
void rt_fmt_hhmm(uint32_t s, char *out, uint32_t cap);
/* "1H 05M" or "12M 30S". */
void rt_fmt_duration(uint32_t s, char *out, uint32_t cap);

#endif /* RT_FORMAT_H */
