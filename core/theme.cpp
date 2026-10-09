#include "theme.h"
#include <cstring>

namespace rotos {

static constexpr uint16_t hex(uint32_t rgb) {
    return uint16_t(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

// Night: Catppuccin-dark map, Hyprland default border colours.
const Theme THEME_NIGHT = {"night", {
    hex(0x11111b), hex(0x181825), hex(0x1b2e26), hex(0x1a2340),
    hex(0x262637), hex(0x07070c), hex(0x45475a), hex(0x7f849c),
    hex(0x05050a), hex(0x33ccff), hex(0x3a3d55), hex(0x0e0e16),
    hex(0x33ccff), hex(0xcdd6f4), hex(0x7f849c), hex(0x00ff99),
}, PanelStyle::Gradient};

// Day: bright, high contrast for sunlight, Pokemon overworld colours.
// Land is kept a step darker than the white roads, and road outlines are
// near-black, so roads stay separate even with glare washing the screen out.
const Theme THEME_DAY = {"day", {
    hex(0xd8d2c0), hex(0xcbc4b0), hex(0x88c470), hex(0x5aa0ea),
    hex(0xb0a494), hex(0x34343c), hex(0xffffff), hex(0xf8c040),
    hex(0x101018), hex(0x2058e8), hex(0x9ca0b0), hex(0xf8f8f0),
    hex(0x2058e8), hex(0x101018), hex(0x686870), hex(0xe83838),
}, PanelStyle::Double};

void blendThemes(const Theme& a, const Theme& b, float t, Theme& out) {
    auto mix = [t](int x, int y) { return int(x + (y - x) * t + 0.5f); };
    for (int i = 0; i < 16; ++i) {
        uint16_t ca = a.rgb565[i], cb = b.rgb565[i];
        int r = mix(ca >> 11, cb >> 11), g = mix((ca >> 5) & 63, (cb >> 5) & 63), bl = mix(ca & 31, cb & 31);
        out.rgb565[i] = uint16_t(r << 11 | g << 5 | bl);
    }
    out.panel = t < 0.5f ? a.panel : b.panel;
    out.name = t < 0.5f ? a.name : b.name;
}

const Theme* themeByName(const char* name) {
    return std::strcmp(name, "day") == 0 ? &THEME_DAY : &THEME_NIGHT;
}

} // namespace rotos
