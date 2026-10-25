/*
 * rt_theme.c - palettes.
 */
#include "rt_theme.h"

#include "rn/rn_types.h"

#define HEX(rgb) ((uint16_t)((((rgb) >> 8) & 0xF800u) | (((rgb) >> 5) & 0x07E0u) | (((rgb) >> 3) & 0x001Fu)))

/* Night: Google Maps' dark map (blue-grey roads on near-black land),
 * Google blue route, Hyprland-style UI. */
const RtTheme RT_THEME_NIGHT = {
    {HEX(0x1f2125u), HEX(0x23262bu), HEX(0x1e3a2bu), HEX(0x16314au),
     HEX(0x2a2e35u), HEX(0x0f1114u), HEX(0x5e6b7eu), HEX(0x8c9cb4u),
     HEX(0x05050au), HEX(0x4285f4u), HEX(0x3a3d55u), HEX(0x0e0e16u),
     HEX(0x33ccffu), HEX(0xcdd6f4u), HEX(0x7f849cu), HEX(0x00ff99u)},
    RT_PANEL_GRADIENT};

/* Day: tuned for the 2.8" LCD, not copied from Google's phone colours
 * (light blue-grey streets on near-white land washed out on the panel and
 * left only jagged outlines): white streets with a grey edge on light grey
 * land, amber main roads, mint parks, blue water, Google blue route. */
const RtTheme RT_THEME_DAY = {
    {HEX(0xe6e4dfu), HEX(0xdedcd6u), HEX(0xbfe8c9u), HEX(0x8ccfe6u),
     HEX(0xd8d6d0u), HEX(0x9aa4b2u), HEX(0xffffffu), HEX(0xffd36eu),
     HEX(0x101018u), HEX(0x1a73e8u), HEX(0x9aa0a6u), HEX(0xf8f8f0u),
     HEX(0x2058e8u), HEX(0x101018u), HEX(0x686870u), HEX(0xe83838u)},
    RT_PANEL_DOUBLE};

void rt_rgb565_split(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!RN_ASSERT(r != NULL && g != NULL && b != NULL)) {
        return;
    }
    uint32_t r5 = (uint32_t)c >> 11;
    uint32_t g6 = ((uint32_t)c >> 5) & 63u;
    uint32_t b5 = (uint32_t)c & 31u;
    *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    *g = (uint8_t)((g6 << 2) | (g6 >> 4));
    *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

uint16_t rt_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)((((uint32_t)r & 0xF8u) << 8) | (((uint32_t)g & 0xFCu) << 3) | ((uint32_t)b >> 3));
}

static uint32_t mix(uint32_t x, uint32_t y, uint32_t t)
{
    return (x * (255u - t) + y * t + 127u) / 255u;
}

void rt_theme_blend(const RtTheme *a, const RtTheme *b, uint8_t t, RtTheme *out)
{
    if (!RN_ASSERT(a != NULL && b != NULL && out != NULL)) {
        return;
    }
    for (uint32_t i = 0u; i < (uint32_t)RT_C_COUNT; i++) {
        uint32_t ca = a->rgb565[i];
        uint32_t cb = b->rgb565[i];
        uint32_t r = mix(ca >> 11, cb >> 11, t);
        uint32_t g = mix((ca >> 5) & 63u, (cb >> 5) & 63u, t);
        uint32_t bl = mix(ca & 31u, cb & 31u, t);
        out->rgb565[i] = (uint16_t)((r << 11) | (g << 5) | bl);
    }
    out->panel = (t < 128u) ? a->panel : b->panel;
}
