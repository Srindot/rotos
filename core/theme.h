// 16-colour palettes. Every pixel ROTOS draws is a 4-bit index into one of
// these, NES/Game Boy style: switching theme recolours everything for free,
// and a full frame is 37.5 KiB instead of 150 KiB.
#pragma once
#include <cstdint>

namespace rotos {

enum Color : uint8_t {
    // 0..7: map classes, produced by the phone's quantizer (phone/rotos_phone/quantize.py)
    C_LAND, C_LANDUSE, C_PARK, C_WATER, C_BUILDING, C_ROAD_EDGE, C_ROAD_MINOR, C_ROAD_MAJOR,
    // 8..15: UI
    C_SHADOW, C_ROUTE, C_ROUTE_DONE, C_PANEL, C_ACCENT, C_TEXT, C_TEXT_DIM, C_HILITE,
};

enum class PanelStyle : uint8_t {
    Gradient,   // Hyprland: 1 px border dithered ACCENT -> HILITE
    Double,     // Pokemon dialog box: dark outer + accent inner
};

struct Theme {
    const char* name;
    uint16_t rgb565[16];
    PanelStyle panel;
};

extern const Theme THEME_NIGHT;
extern const Theme THEME_DAY;
const Theme* themeByName(const char* name);

// Palette crossfade, NES style: blend every entry, t = 0 -> a, 1 -> b.
// The panel style flips at the halfway point.
void blendThemes(const Theme& a, const Theme& b, float t, Theme& out);

} // namespace rotos
