/*
 * hal_display.h - the display seam: an ILI9341 behind an SPI bus with a D/C
 * pin, plus the backlight.
 *
 * Implementations:
 *   firmware/hal/esp/hal_display_esp.c   ESP32 SPI2 + DMA, LEDC backlight
 *   sim/host/hal_display_sim.c           virtual SPI over TCP to the screen twin
 *
 * Contract for hal_display_data(): the transfer may still be running when the
 * call returns, but the HAL keeps at most one transfer in flight and waits
 * for it before starting the next one, so the caller may reuse a buffer
 * after the *following* call (rt_present alternates two band buffers).
 */
#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

bool hal_display_init(void);
/* Hardware reset pulse on the RST pin (no-op on the twin). */
void hal_display_reset(void);
/* D/C low: one command byte. */
void hal_display_command(uint8_t cmd);
/* D/C high: parameter or pixel bytes. */
void hal_display_data(const uint8_t *bytes, uint32_t n);
/* Wait until every queued transfer has finished. */
void hal_display_flush(void);
/* Backlight 0..100 %. */
void hal_display_backlight(uint8_t pct);
/* MADCTL value that makes this board's panel a 240 x 320 portrait screen,
 * right way up (modules differ in how the glass is wired to the controller). */
uint8_t hal_display_madctl(void);

#endif /* HAL_DISPLAY_H */
