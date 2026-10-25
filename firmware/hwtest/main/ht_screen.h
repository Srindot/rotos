/*
 * ht_screen.h - what the hardware test draws.
 *
 *   sweep   full-screen RED, GREEN, BLUE, WHITE, BLACK, each named in text
 *   test    title, white border on all four edges, colour bars, one box per
 *           button (lights up green while held, with a press count), uptime
 *   chart   full-screen orientation chart: four coloured quadrants named by
 *           corner, a 40 px grid numbered along the top and left edges, and
 *           an UP arrow, so a photo shows exactly which part of the frame lands
 *           where on the glass; the orientation mode is printed under UP
 */
#ifndef HT_SCREEN_H
#define HT_SCREEN_H

#include <stdint.h>

#include "ht_buttons.h"
#include "rt_canvas.h"

void ht_screen_sweep(RtCanvas *cv, uint32_t step);
void ht_screen_chart(RtCanvas *cv, uint32_t mode, uint8_t madctl);
void ht_screen_test(RtCanvas *cv, const HtButtons *b, uint32_t uptime_s, uint32_t fps);

#endif /* HT_SCREEN_H */
