// Hand-drawn 5x7 pixel font, scaled by integers only (1x labels, 2x values,
// 4x the big distance/speed numbers). Upper-case only, like an old handheld.
// '~' draws a degree sign ("27~C").
#pragma once
#include "canvas.h"

namespace rotos {

// Width in pixels of `s` at `scale`. Digits are fixed-width so countdowns
// do not jitter; everything else is proportional.
int textWidth(const char* s, int scale = 1);
// Draws `s`, returns the x after the last glyph. `shadow` draws a 1-unit
// drop shadow in C_SHADOW first: keeps text readable over any map colour.
int drawText(Canvas& cv, int x, int y, const char* s, int scale, uint8_t color, bool shadow = false);
int drawTextCentered(Canvas& cv, int cx, int y, const char* s, int scale, uint8_t color, bool shadow = false);

} // namespace rotos
