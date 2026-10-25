/*
 * rt_ili9341.h - ILI9341 driver (240 x 320, RGB565, portrait), written only
 * against hal_display.h so it runs unchanged on the screen twin and on the
 * real panel.
 */
#ifndef RT_ILI9341_H
#define RT_ILI9341_H

#include <stdint.h>

#define RT_ILI_SWRESET 0x01u
#define RT_ILI_SLPOUT  0x11u
#define RT_ILI_DISPON  0x29u
#define RT_ILI_CASET   0x2Au
#define RT_ILI_PASET   0x2Bu
#define RT_ILI_RAMWR   0x2Cu
#define RT_ILI_MADCTL  0x36u
#define RT_ILI_COLMOD  0x3Au

/* Full power-up sequence (reset, power control, gamma, 16 bpp, portrait). */
/* Hardware reset + the full init sequence (~450 ms). */
void rt_ili9341_init(void);
/* Re-send the configuration without resetting (~6 ms, invisible on a working
 * panel). A panel that reset itself on a supply dip (engine cranking) comes
 * back to life; the caller then redraws the whole frame. */
#define RT_ILI_QUICK_WAIT_MS 5u
void rt_ili9341_refresh(void);
/* Address window, inclusive, then RAMWR: pixel data may follow. */
void rt_ili9341_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

#endif /* RT_ILI9341_H */
