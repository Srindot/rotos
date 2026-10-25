/*
 * ht_palette.h - the hardware test's 16 colours.
 *
 * Plain primaries, so a red/blue swap or a wrong colour depth is obvious:
 * the word "RED" must be drawn on a red screen.
 */
#ifndef HT_PALETTE_H
#define HT_PALETTE_H

#include "rt_theme.h"

enum {
    HT_BLACK = 0,
    HT_RED,
    HT_GREEN,
    HT_BLUE,
    HT_WHITE,
    HT_YELLOW,
    HT_CYAN,
    HT_MAGENTA,
    HT_GREY,
    HT_DARK,
    HT_ORANGE
};

extern const RtTheme HT_PALETTE;

/* Full-screen colour steps shown at boot. */
#define HT_SWEEP_COUNT 5u
uint8_t ht_sweep_color(uint32_t step);
const char *ht_sweep_name(uint32_t step);

#endif /* HT_PALETTE_H */
