/*
 * rt_settings.c - the persistent 16-byte record:
 *   [0..2]   'R' 'S' version
 *   [3..6]   SETTINGS payload (theme, north-up, glass %, brightness %)
 *   [7..10]  odometer, u32 little-endian, units of 10 m
 *   [11..12] last ride distance, u16, units of 10 m
 *   [13..14] last ride duration, u16, minutes
 *   [15]     finished rides, saturating at 255
 * Settings and statistics are written with read-modify-write, so neither
 * clobbers the other. Records from before the statistics read as zeros.
 */
#include "rt_settings.h"

#include <string.h>

#include "hal_storage.h"
#include "rn/rn_types.h"
#include "rt_config.h"

#define REC_VERSION 1u

RnSettings rt_settings_defaults(void)
{
    RnSettings s = {RN_THEME_AUTO, 0u, (uint8_t)RT_GLASS_DEFAULT, 0u};
    return s;
}

/* The stored record, or false when there is none / it is not ours. */
static bool load_record(uint8_t rec[HAL_SETTINGS_SIZE])
{
    (void)memset(rec, 0, HAL_SETTINGS_SIZE);
    if (!hal_storage_settings_load(rec, HAL_SETTINGS_SIZE)) {
        return false;
    }
    return rec[0] == (uint8_t)'R' && rec[1] == (uint8_t)'S' && rec[2] == REC_VERSION;
}

static uint32_t rd16(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static void wr16(uint8_t *p, uint32_t v)
{
    uint32_t c = (v > 0xFFFFu) ? 0xFFFFu : v;
    p[0] = (uint8_t)(c & 0xFFu);
    p[1] = (uint8_t)(c >> 8);
}

RnSettings rt_settings_load(void)
{
    uint8_t rec[HAL_SETTINGS_SIZE];
    RnSettings s = rt_settings_defaults();
    if (!load_record(rec)) {
        return s;
    }
    bool ok = rec[3] <= RN_THEME_NIGHT && rec[4] <= 1u && rec[5] <= 100u && rec[6] <= 100u;
    if (ok) {
        s.theme_mode = rec[3];
        s.north_up = rec[4];
        s.glass_pct = rec[5];
        s.brightness_pct = rec[6];
    }
    (void)RN_ASSERT(s.glass_pct <= 100u);
    return s;
}

bool rt_settings_save(const RnSettings *s)
{
    if (!RN_ASSERT(s != NULL)) {
        return false;
    }
    uint8_t rec[HAL_SETTINGS_SIZE];
    if (!load_record(rec)) {
        (void)memset(rec, 0, sizeof rec);             /* no statistics yet */
    }
    rec[0] = (uint8_t)'R';
    rec[1] = (uint8_t)'S';
    rec[2] = REC_VERSION;
    rec[3] = s->theme_mode;
    rec[4] = s->north_up;
    rec[5] = s->glass_pct;
    rec[6] = s->brightness_pct;
    return hal_storage_settings_save(rec, sizeof rec);
}

RtStats rt_stats_load(void)
{
    uint8_t rec[HAL_SETTINGS_SIZE];
    RtStats st = {0u, 0u, 0u, 0u};
    if (!load_record(rec)) {
        return st;
    }
    st.odo_m = (rd16(&rec[7]) | (rd16(&rec[9]) << 16)) * 10u;
    st.last_m = rd16(&rec[11]) * 10u;
    st.last_min = rd16(&rec[13]);
    st.rides = rec[15];
    return st;
}

bool rt_stats_add_ride(RtStats *st, float distance_m, uint32_t ride_s)
{
    if (!RN_ASSERT(st != NULL) || !RN_ASSERT(distance_m >= 0.0f)) {
        return false;
    }
    uint32_t m = (distance_m < 4.0e8f) ? (uint32_t)distance_m : 400000000u;
    st->odo_m += m;
    st->last_m = m;
    st->last_min = (ride_s + 30u) / 60u;
    st->rides = (st->rides < 255u) ? st->rides + 1u : 255u;
    uint8_t rec[HAL_SETTINGS_SIZE];
    if (!load_record(rec)) {
        RnSettings d = rt_settings_defaults();
        (void)memset(rec, 0, sizeof rec);
        rec[0] = (uint8_t)'R';
        rec[1] = (uint8_t)'S';
        rec[2] = REC_VERSION;
        rec[3] = d.theme_mode;
        rec[5] = d.glass_pct;
    }
    uint32_t odo10 = st->odo_m / 10u;
    wr16(&rec[7], odo10 & 0xFFFFu);
    wr16(&rec[9], odo10 >> 16);
    wr16(&rec[11], st->last_m / 10u);
    wr16(&rec[13], st->last_min);
    rec[15] = (uint8_t)st->rides;
    return hal_storage_settings_save(rec, sizeof rec);
}
