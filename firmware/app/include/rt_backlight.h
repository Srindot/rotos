/*
 * rt_backlight.h - backlight level from the ambient light sensor (or a
 * fixed day / night level without one), smoothed so it never flickers.
 */
#ifndef RT_BACKLIGHT_H
#define RT_BACKLIGHT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float level;
    bool init;
    uint32_t last_ms;
    uint8_t applied;
} RtBacklight;

void rt_backlight_init(RtBacklight *b);
/* light: 0..100 or -1 (no sensor); setting_pct: 0 = automatic.
 * Returns the backlight % to apply. */
uint8_t rt_backlight_update(RtBacklight *b, int32_t light, uint8_t setting_pct, bool night, uint32_t now_ms);

#endif /* RT_BACKLIGHT_H */
