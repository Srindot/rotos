/*
 * rt_icons.c - sprite drawing.
 */
#include "rt_icons.h"

#include "rn/rn_types.h"
#include "rt_sprites.h"

/* Palette index for a sprite character, or -1 = transparent. */
static int32_t sprite_color(char ch, uint8_t ink)
{
    switch (ch) {
    case '#': return (int32_t)ink;
    case '+': return RT_C_ACCENT;
    case 's': return RT_C_SHADOW;
    case 'a': return RT_C_ACCENT;
    case 'w': return RT_C_TEXT;
    case 'p': return RT_C_SHADOW;
    case 'h': return RT_C_HILITE;
    default:  return -1;
    }
}

static void draw_sprite(RtCanvas *cv, int32_t x, int32_t y, const char *const *s, bool mirror,
                        int32_t scale, uint8_t ink)
{
    if (!RN_ASSERT(cv != NULL && s != NULL) || !RN_ASSERT(scale > 0 && scale <= 8)) {
        return;
    }
    for (int32_t r = 0; r < RT_SPRITE; r++) {
        for (int32_t c = 0; c < RT_SPRITE; c++) {
            int32_t col = sprite_color(s[r][mirror ? RT_SPRITE - 1 - c : c], ink);
            if (col >= 0) {
                rt_canvas_fill_rect(cv, x + c * scale, y + r * scale, scale, scale, (uint8_t)col);
            }
        }
    }
}

void rt_icon_maneuver(RtCanvas *cv, int32_t x, int32_t y, uint8_t type, int32_t scale, uint8_t color)
{
    bool mirror = false;
    const char *const *s = rt_sprite_maneuver(type, &mirror);
    draw_sprite(cv, x, y, s, mirror, scale, color);
}

void rt_icon_weather(RtCanvas *cv, int32_t x, int32_t y, uint8_t wmo, bool is_day, int32_t scale)
{
    draw_sprite(cv, x, y, rt_sprite_weather(wmo, is_day), false, scale, RT_C_TEXT);
}

const char *rt_weather_name(uint8_t c)
{
    if (c == 0u) { return "CLEAR"; }
    if (c == 1u) { return "MOSTLY CLEAR"; }
    if (c == 2u) { return "PARTLY CLOUDY"; }
    if (c == 3u) { return "CLOUDY"; }
    if (c == 45u || c == 48u) { return "FOG"; }
    if (c >= 51u && c <= 57u) { return "DRIZZLE"; }
    if (c == 65u || c == 82u) { return "HEAVY RAIN"; }
    if (c >= 61u && c <= 67u) { return "RAIN"; }
    if (c >= 80u && c <= 82u) { return "SHOWERS"; }
    if (c >= 95u) { return "THUNDERSTORM"; }
    return "CLOUDY";
}

void rt_icon_flyover_arrow(RtCanvas *cv, int32_t x, int32_t y, bool up, uint8_t color)
{
    static const char *const ARROW[7] = {
        "...#...", "..###..", ".#####.", "#######", "..###..", "..###..", "..###.."};
    if (!RN_ASSERT(cv != NULL)) {
        return;
    }
    for (int32_t r = 0; r < 7; r++) {
        for (int32_t c = 0; c < 7; c++) {
            if (ARROW[up ? r : 6 - r][c] == '#') {
                rt_canvas_pixel(cv, x + c, y + r, color);
            }
        }
    }
}

void rt_icon_mascot(RtCanvas *cv, int32_t x, int32_t y, uint8_t mood, int32_t scale)
{
    draw_sprite(cv, x, y, rt_sprite_mascot(mood), false, scale, RT_C_TEXT);
}

void rt_icon_phone(RtCanvas *cv, int32_t x, int32_t y, int32_t scale, uint8_t color)
{
    draw_sprite(cv, x, y, rt_sprite_phone(), false, scale, color);
}
