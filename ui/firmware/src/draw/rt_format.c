/*
 * rt_format.c - bounded text formatters.
 */
#include "rt_format.h"

#include <math.h>
#include <stdbool.h>

#include "rn/rn_types.h"

#define MAX_DIGITS 10u

uint32_t rt_fmt_cat(char *dst, uint32_t cap, const char *src)
{
    if (!RN_ASSERT(dst != NULL && cap > 0u) || !RN_ASSERT(src != NULL)) {
        return 0u;
    }
    uint32_t n = 0u;
    while (n + 1u < cap && dst[n] != '\0') {
        n++;
    }
    for (uint32_t i = 0u; n + 1u < cap && src[i] != '\0' && i < 256u; i++) {
        dst[n] = src[i];
        n++;
    }
    dst[n] = '\0';
    return n;
}

uint32_t rt_fmt_u32(char *dst, uint32_t cap, uint32_t v, uint32_t min_digits)
{
    if (!RN_ASSERT(dst != NULL && cap > 1u) || !RN_ASSERT(min_digits <= MAX_DIGITS)) {
        return 0u;
    }
    char tmp[MAX_DIGITS];
    uint32_t n = 0u;
    uint32_t x = v;
    while (n < MAX_DIGITS && (x > 0u || n < min_digits || n == 0u)) {
        tmp[n] = (char)('0' + (char)(x % 10u));
        x /= 10u;
        n++;
    }
    uint32_t out = 0u;
    while (n > 0u && out + 1u < cap) {
        n--;
        dst[out] = tmp[n];
        out++;
    }
    dst[out] = '\0';
    return out;
}

uint32_t rt_fmt_round(char *dst, uint32_t cap, float v)
{
    if (!RN_ASSERT(dst != NULL && cap > 2u) || !RN_ASSERT(isfinite(v))) {
        return 0u;
    }
    float r = roundf(v);
    if (r < 0.0f) {
        dst[0] = '-';
        return 1u + rt_fmt_u32(&dst[1], cap - 1u, (uint32_t)(-r), 1u);
    }
    return rt_fmt_u32(dst, cap, (uint32_t)r, 1u);
}

void rt_fmt_distance(float m, char *value, char *unit)
{
    if (!RN_ASSERT(value != NULL && unit != NULL)) {
        return;
    }
    float d = (m > 0.0f && isfinite(m)) ? m : 0.0f;
    value[0] = '\0';
    unit[0] = '\0';
    if (d < 1000.0f) {
        uint32_t r = (d < 100.0f) ? (uint32_t)(d / 5.0f + 0.5f) * 5u : (uint32_t)(d / 10.0f + 0.5f) * 10u;
        if (r >= 1000u) {
            (void)rt_fmt_cat(value, RT_VALUE_LEN, "1.0");
            (void)rt_fmt_cat(unit, RT_UNIT_LEN, "KM");
            return;
        }
        (void)rt_fmt_u32(value, RT_VALUE_LEN, r, 1u);
        (void)rt_fmt_cat(unit, RT_UNIT_LEN, "M");
    } else if (d < 10000.0f) {
        uint32_t tenths = (uint32_t)(d / 100.0f + 0.5f);
        (void)rt_fmt_u32(value, RT_VALUE_LEN, tenths / 10u, 1u);
        (void)rt_fmt_cat(value, RT_VALUE_LEN, ".");
        char frac[4];
        (void)rt_fmt_u32(frac, sizeof frac, tenths % 10u, 1u);
        (void)rt_fmt_cat(value, RT_VALUE_LEN, frac);
        (void)rt_fmt_cat(unit, RT_UNIT_LEN, "KM");
    } else {
        (void)rt_fmt_u32(value, RT_VALUE_LEN, (uint32_t)(d / 1000.0f + 0.5f), 1u);
        (void)rt_fmt_cat(unit, RT_UNIT_LEN, "KM");
    }
}

void rt_fmt_hhmm(uint32_t s, char *out, uint32_t cap)
{
    if (!RN_ASSERT(out != NULL && cap >= 6u)) {
        return;
    }
    char mm[4];
    out[0] = '\0';
    (void)rt_fmt_u32(out, cap, (s / 3600u) % 24u, 2u);
    (void)rt_fmt_cat(out, cap, ":");
    (void)rt_fmt_u32(mm, sizeof mm, (s / 60u) % 60u, 2u);
    (void)rt_fmt_cat(out, cap, mm);
}

void rt_fmt_duration(uint32_t s, char *out, uint32_t cap)
{
    if (!RN_ASSERT(out != NULL && cap >= 12u)) {
        return;
    }
    char b[8];
    out[0] = '\0';
    bool hours = s >= 3600u;
    (void)rt_fmt_u32(out, cap, hours ? s / 3600u : s / 60u, 1u);
    (void)rt_fmt_cat(out, cap, hours ? "H " : "M ");
    (void)rt_fmt_u32(b, sizeof b, hours ? (s / 60u) % 60u : s % 60u, 2u);
    (void)rt_fmt_cat(out, cap, b);
    (void)rt_fmt_cat(out, cap, hours ? "M" : "S");
}
