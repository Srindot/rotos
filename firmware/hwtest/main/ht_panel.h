/*
 * ht_panel.h - try the 8 panel orientations (ILI9341 MADCTL).
 *
 * The module's controller turned out to be 320 columns x 240 rows (the
 * ruler showed all 8 columns of 40 px). Modes 0-3 set the row/column
 * exchange bit, which gives the 240 x 320 portrait space the canvas is
 * drawn in, four ways round; modes 4-7 are the non-exchanged ones.
 */
#ifndef HT_PANEL_H
#define HT_PANEL_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_canvas.h"
#include "rt_present.h"
#include "rt_theme.h"

#define HT_MODE_COUNT 8u

void ht_panel_set_mode(uint32_t mode);
/* Mode index for a MADCTL value (0 if it is not one of the 8). */
uint32_t ht_panel_mode_of(uint8_t madctl);
uint8_t ht_panel_madctl(uint32_t mode);
/* Fill the panel's whole memory with one colour (portrait, mode 0). The
 * window is 320 x 480, larger than an ILI9341's 240 x 320: the controller
 * ignores the out-of-range part, and a bigger panel still gets covered. */
void ht_panel_fill(uint16_t rgb565);
/* Ruler over the same 320 x 480 area, mode 0: 8 columns of 40 px, each its
 * own colour (white, red, orange, yellow, green, cyan, blue, magenta, left
 * to right), and 12 rows of 40 px alternating colour / black. Counting what
 * is visible gives the panel's real size and which way it is wired. */
void ht_panel_ruler(void);
void ht_panel_push(RtPresenter *p, const RtCanvas *cv, const RtTheme *t, uint32_t mode, bool force);

#endif /* HT_PANEL_H */
