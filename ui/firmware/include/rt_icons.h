/*
 * rt_icons.h - draws sprites at integer scale.
 */
#ifndef RT_ICONS_H
#define RT_ICONS_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_canvas.h"

void rt_icon_maneuver(RtCanvas *cv, int32_t x, int32_t y, uint8_t type, int32_t scale, uint8_t color);
void rt_icon_weather(RtCanvas *cv, int32_t x, int32_t y, uint8_t wmo, bool is_day, int32_t scale);
const char *rt_weather_name(uint8_t wmo);
/* 7x7 up / down arrow for the flyover chip. */
void rt_icon_flyover_arrow(RtCanvas *cv, int32_t x, int32_t y, bool up, uint8_t color);
void rt_icon_phone(RtCanvas *cv, int32_t x, int32_t y, int32_t scale, uint8_t color);
void rt_icon_mascot(RtCanvas *cv, int32_t x, int32_t y, uint8_t mood, int32_t scale);

#endif /* RT_ICONS_H */
