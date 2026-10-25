/*
 * rt_present.h - sends the 4-bit canvas to the panel.
 *
 * The canvas is expanded to RGB565 through the active palette in 16-row
 * bands. A band is only sent when its content (pixels + glass placement)
 * hashed differently from the last frame, so a still screen costs no SPI
 * time at all, and a moving map costs ~29 ms at 40 MHz.
 */
#ifndef RT_PRESENT_H
#define RT_PRESENT_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_canvas.h"
#include "rt_theme.h"

typedef struct {
    uint32_t band_hash[RT_BANDS];
    uint16_t pal[RT_C_COUNT];
    uint16_t glass_pal[RT_C_COUNT];
    bool have_pal;
    int32_t glass_pct;
    int32_t built_pct;
    uint8_t line[2][RT_SCREEN_W * RT_BAND_H * 2];   /* ping-pong DMA buffers */
    uint32_t which;
    uint32_t bands_sent;                            /* statistics           */
    uint32_t bytes_sent;
} RtPresenter;

void rt_present_init(RtPresenter *p);
void rt_present_set_glass(RtPresenter *p, int32_t pct);
/* force = resend every band (e.g. after a display reset). */
void rt_present_frame(RtPresenter *p, const RtCanvas *cv, const RtTheme *t, bool force);
/* Row y exactly as the panel receives it, native-endian RGB565 (also used
 * for simulator screenshots, so the two can never disagree). */
void rt_present_row(const RtPresenter *p, const RtCanvas *cv, int32_t y, uint16_t *out);

#endif /* RT_PRESENT_H */
