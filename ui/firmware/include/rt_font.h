/*
 * rt_font.h - hand-drawn 5x7 pixel font, integer scaling only (1x labels,
 * 2x values, 4x-6x big numbers). Upper-case only, like an old handheld.
 * '~' draws a degree sign ("27~C"). Digits are fixed-width so countdowns do
 * not jitter; everything else is proportional.
 */
#ifndef RT_FONT_H
#define RT_FONT_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_canvas.h"

#define RT_FONT_H_PX 7

int32_t rt_text_width(const char *s, int32_t scale);
/* Returns the x just after the last glyph. shadow draws a 1-unit drop
 * shadow in RT_C_SHADOW first (readable over any map colour). */
int32_t rt_text(RtCanvas *cv, int32_t x, int32_t y, const char *s, int32_t scale, uint8_t color, bool shadow);
int32_t rt_text_centered(RtCanvas *cv, int32_t cx, int32_t y, const char *s, int32_t scale, uint8_t color, bool shadow);

#endif /* RT_FONT_H */
