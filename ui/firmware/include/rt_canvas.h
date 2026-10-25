/*
 * rt_canvas.h - 4-bit indexed framebuffer (240 x 320 -> 38,400 bytes) and
 * drawing primitives. Every coordinate is clipped; nothing writes outside
 * the buffer.
 *
 * Glass windows are SNES/GBA-style "colour math" regions: inside a window,
 * map and route pixels are shown through a second, pre-blended palette
 * (dimmed toward the panel colour) at present time, so panels look frosted
 * without an alpha channel or a second framebuffer.
 */
#ifndef RT_CANVAS_H
#define RT_CANVAS_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_colors.h"
#include "rt_config.h"
#include "rt_theme.h"

#define RT_STRIDE     (RT_SCREEN_W / 2)
#define RT_MAX_GLASS  8

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} RtRect;

typedef struct {
    uint8_t px[RT_STRIDE * RT_SCREEN_H];
    RtRect glass[RT_MAX_GLASS];
    uint32_t n_glass;
} RtCanvas;

/* Call at the start of every frame: forgets last frame's glass windows. */
void rt_canvas_begin(RtCanvas *cv);
void rt_canvas_clear(RtCanvas *cv, uint8_t c);
void rt_canvas_pixel(RtCanvas *cv, int32_t x, int32_t y, uint8_t c);
uint8_t rt_canvas_get(const RtCanvas *cv, int32_t x, int32_t y);
void rt_canvas_hline(RtCanvas *cv, int32_t x0, int32_t x1, int32_t y, uint8_t c);
void rt_canvas_fill_rect(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c);
void rt_canvas_rect(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c);
void rt_canvas_fill_circle(RtCanvas *cv, int32_t cx, int32_t cy, int32_t r, uint8_t c);
void rt_canvas_fill_triangle(RtCanvas *cv, const float *xs, const float *ys, uint8_t c);
/* Round-capped line of radius r (r = 0: 1 px). */
void rt_canvas_thick_line(RtCanvas *cv, float x0, float y0, float x1, float y1, int32_t r, uint8_t c);
/* 50 % checkerboard: the classic "semi-transparent" shade without alpha. */
void rt_canvas_dither_rect(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c);
/* Register a glass window (map shows through, dimmed). */
void rt_canvas_glass(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h);

#endif /* RT_CANVAS_H */
