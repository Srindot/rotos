/*
 * ht_palette.c - test colours and the boot colour sweep.
 */
#include "ht_palette.h"

#include "rn/rn_types.h"

#define HEX(rgb) ((uint16_t)((((rgb) >> 8) & 0xF800u) | (((rgb) >> 5) & 0x07E0u) | (((rgb) >> 3) & 0x001Fu)))

const RtTheme HT_PALETTE = {
    {HEX(0x000000u), HEX(0xff0000u), HEX(0x00ff00u), HEX(0x0000ffu),
     HEX(0xffffffu), HEX(0xffff00u), HEX(0x00ffffu), HEX(0xff00ffu),
     HEX(0x808080u), HEX(0x202020u), HEX(0xff8000u), HEX(0x000000u),
     HEX(0x000000u), HEX(0x000000u), HEX(0x000000u), HEX(0x000000u)},
    (uint8_t)RT_PANEL_GRADIENT,
};

static const uint8_t SWEEP_COLOR[HT_SWEEP_COUNT] = {HT_RED, HT_GREEN, HT_BLUE, HT_WHITE, HT_BLACK};
static const char *const SWEEP_NAME[HT_SWEEP_COUNT] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};

uint8_t ht_sweep_color(uint32_t step)
{
    if (!RN_ASSERT(step < HT_SWEEP_COUNT)) {
        return HT_BLACK;
    }
    return SWEEP_COLOR[step];
}

const char *ht_sweep_name(uint32_t step)
{
    if (!RN_ASSERT(step < HT_SWEEP_COUNT)) {
        return "?";
    }
    return SWEEP_NAME[step];
}
