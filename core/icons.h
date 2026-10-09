// 16x16 1-bit maneuver icons, drawn at integer scale. Right-hand variants
// are the left-hand ones mirrored, saving half the sprite memory.
#pragma once
#include "canvas.h"
#include "nav.h"

namespace rotos {

void drawManeuverIcon(Canvas& cv, int x, int y, ManeuverType t, int scale, uint8_t color);

// 16x16 weather icon for a WMO weather code. Monochrome like Waybar icons:
// shapes in C_TEXT, rain / lightning in C_ACCENT.
void drawWeatherIcon(Canvas& cv, int x, int y, uint8_t wmoCode, bool isDay, int scale);
const char* weatherName(uint8_t wmoCode);

// 7x7 up / down arrow for the flyover chip.
void drawFlyoverArrow(Canvas& cv, int x, int y, bool up, uint8_t color);

// Dashboard mascot. PLACEHOLDER design: an original little robot until the
// real mascot is chosen; swap the sprite data in icons.cpp, nothing else changes.
// mood: 0 idle, 1 blink, 2 happy (ride finished).
void drawMascot(Canvas& cv, int x, int y, int mood, int scale);

} // namespace rotos
