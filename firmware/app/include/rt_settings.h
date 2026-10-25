/*
 * rt_settings.h - user settings: persisted on the device, changed by the
 * buttons or by a SETTINGS packet from the phone.
 */
#ifndef RT_SETTINGS_H
#define RT_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_protocol.h"

/* Defaults: automatic theme, heading-up, 70 % glass, automatic brightness. */
RnSettings rt_settings_defaults(void);
/* Stored settings, or the defaults when nothing valid is stored. */
RnSettings rt_settings_load(void);
bool rt_settings_save(const RnSettings *s);

/* Ride statistics kept in the same 16-byte record (bytes 7..15), shown on
 * the idle screen when no phone is connected. */
typedef struct {
    uint32_t odo_m;           /* all rides, metres (10 m resolution)    */
    uint32_t last_m;          /* last ride distance                     */
    uint32_t last_min;        /* last ride duration, minutes            */
    uint32_t rides;           /* number of finished rides (max 255)     */
} RtStats;

RtStats rt_stats_load(void);
/* Adds a finished ride to [st] and stores it; false if the write failed. */
bool rt_stats_add_ride(RtStats *st, float distance_m, uint32_t ride_s);

#endif /* RT_SETTINGS_H */
