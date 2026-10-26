/*
 * rt_canvas.c - framebuffer primitives.
 */
#include "rt_canvas.h"

#include <math.h>
#include <string.h>

#include "rn/rn_types.h"

#define MAX_LINE_STEPS 4000    /* longer lines come from points far off screen */

void rt_canvas_begin(RtCanvas *cv)
{
    if (!RN_ASSERT(cv != NULL)) {
        return;
    }
    cv->n_glass = 0u;
}

void rt_canvas_clear(RtCanvas *cv, uint8_t c)
{
    if (!RN_ASSERT(cv != NULL) || !RN_ASSERT(c < RT_C_COUNT)) {
        return;
    }
    (void)memset(cv->px, (int)((c << 4) | c), sizeof cv->px);
}

void rt_canvas_pixel(RtCanvas *cv, int32_t x, int32_t y, uint8_t c)
{
    if (x < 0 || y < 0 || x >= RT_SCREEN_W || y >= RT_SCREEN_H) {
        return;
    }
    uint8_t *b = &cv->px[y * RT_STRIDE + (x >> 1)];
    *b = ((x & 1) != 0) ? (uint8_t)((*b & 0xF0u) | (c & 0x0Fu)) : (uint8_t)((*b & 0x0Fu) | (uint8_t)(c << 4));
}

uint8_t rt_canvas_get(const RtCanvas *cv, int32_t x, int32_t y)
{
    if (!RN_ASSERT(cv != NULL) || x < 0 || y < 0 || x >= RT_SCREEN_W || y >= RT_SCREEN_H) {
        return 0u;
    }
    uint8_t b = cv->px[y * RT_STRIDE + (x >> 1)];
    return ((x & 1) != 0) ? (uint8_t)(b & 0x0Fu) : (uint8_t)(b >> 4);
}

void rt_canvas_hline(RtCanvas *cv, int32_t x0, int32_t x1, int32_t y, uint8_t c)
{
    if (!RN_ASSERT(cv != NULL) || y < 0 || y >= RT_SCREEN_H) {
        return;
    }
    int32_t a = (x0 < x1) ? x0 : x1;
    int32_t b = (x0 < x1) ? x1 : x0;
    a = (a < 0) ? 0 : a;
    b = (b >= RT_SCREEN_W) ? RT_SCREEN_W - 1 : b;
    if (a > b) {
        return;
    }
    if ((a & 1) != 0) {
        rt_canvas_pixel(cv, a, y, c);
        a++;
    }
    if ((b & 1) == 0) {
        rt_canvas_pixel(cv, b, y, c);
        b--;
    }
    if (a < b) {
        (void)memset(&cv->px[y * RT_STRIDE + (a >> 1)], (int)((c << 4) | c), (size_t)((b - a + 1) >> 1));
    }
}

void rt_canvas_fill_rect(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c)
{
    if (!RN_ASSERT(cv != NULL) || w <= 0 || h <= 0) {
        return;
    }
    int32_t y0 = (y < 0) ? 0 : y;
    int32_t y1 = (y + h > RT_SCREEN_H) ? RT_SCREEN_H : y + h;
    for (int32_t j = y0; j < y1; j++) {
        rt_canvas_hline(cv, x, x + w - 1, j, c);
    }
}

void rt_canvas_rect(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c)
{
    if (!RN_ASSERT(cv != NULL) || w <= 0 || h <= 0) {
        return;
    }
    rt_canvas_hline(cv, x, x + w - 1, y, c);
    rt_canvas_hline(cv, x, x + w - 1, y + h - 1, c);
    for (int32_t j = y + 1; j < y + h - 1 && j < RT_SCREEN_H; j++) {
        rt_canvas_pixel(cv, x, j, c);
        rt_canvas_pixel(cv, x + w - 1, j, c);
    }
}

void rt_canvas_dither_rect(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c)
{
    if (!RN_ASSERT(cv != NULL) || w <= 0 || h <= 0) {
        return;
    }
    int32_t y0 = (y < 0) ? 0 : y;
    int32_t y1 = (y + h > RT_SCREEN_H) ? RT_SCREEN_H : y + h;
    for (int32_t j = y0; j < y1; j++) {
        for (int32_t i = x + ((x + j) & 1); i < x + w && i < RT_SCREEN_W; i += 2) {
            rt_canvas_pixel(cv, i, j, c);
        }
    }
}

void rt_canvas_fill_circle(RtCanvas *cv, int32_t cx, int32_t cy, int32_t r, uint8_t c)
{
    if (!RN_ASSERT(cv != NULL) || r < 0 || r > RT_SCREEN_H) {
        return;
    }
    for (int32_t dy = -r; dy <= r; dy++) {
        int32_t dx = (int32_t)(sqrtf((float)(r * r - dy * dy)) + 0.35f);
        rt_canvas_hline(cv, cx - dx, cx + dx, cy + dy, c);
    }
}

static void swapf(float *a, float *b)
{
    float t = *a;
    *a = *b;
    *b = t;
}

void rt_canvas_fill_triangle(RtCanvas *cv, const float *xs, const float *ys, uint8_t c)
{
    if (!RN_ASSERT(cv != NULL && xs != NULL && ys != NULL)) {
        return;
    }
    float x0 = xs[0], y0 = ys[0], x1 = xs[1], y1 = ys[1], x2 = xs[2], y2 = ys[2];
    if (y0 > y1) { swapf(&x0, &x1); swapf(&y0, &y1); }
    if (y1 > y2) { swapf(&x1, &x2); swapf(&y1, &y2); }
    if (y0 > y1) { swapf(&x0, &x1); swapf(&y0, &y1); }
    int32_t ya = (int32_t)ceilf(y0);
    int32_t yb = (int32_t)floorf(y2);
    ya = (ya < 0) ? 0 : ya;
    yb = (yb >= RT_SCREEN_H) ? RT_SCREEN_H - 1 : yb;
    for (int32_t y = ya; y <= yb; y++) {
        float fy = (float)y;
        float xa = (y2 != y0) ? x0 + (x2 - x0) * (fy - y0) / (y2 - y0) : x0;
        float xb;
        if (fy < y1) {
            xb = (y1 != y0) ? x0 + (x1 - x0) * (fy - y0) / (y1 - y0) : x1;
        } else {
            xb = (y2 != y1) ? x1 + (x2 - x1) * (fy - y1) / (y2 - y1) : x1;
        }
        rt_canvas_hline(cv, (int32_t)lroundf(xa), (int32_t)lroundf(xb), y, c);
    }
}

static bool line_visible(float x0, float y0, float x1, float y1, int32_t r)
{
    float fr = (float)r;
    return !(fmaxf(x0, x1) < -fr || fminf(x0, x1) > (float)RT_SCREEN_W + fr ||
             fmaxf(y0, y1) < -fr || fminf(y0, y1) > (float)RT_SCREEN_H + fr);
}

/* Liang-Barsky: clip one parameter bound; false when the segment misses. */
static bool clip_edge(float p, float q, float *t0, float *t1)
{
    if (p == 0.0f) {
        return q >= 0.0f;
    }
    float t = q / p;
    if (p < 0.0f) {
        if (t > *t1) {
            return false;
        }
        *t0 = (t > *t0) ? t : *t0;
    } else {
        if (t < *t0) {
            return false;
        }
        *t1 = (t < *t1) ? t : *t1;
    }
    return true;
}

/* Clips the segment to the screen plus margin m, so a segment ending km
 * off screen (long straight roads) is drawn over its visible part. */
static bool clip_segment(float *x0, float *y0, float *x1, float *y1, float m)
{
    float dx = *x1 - *x0;
    float dy = *y1 - *y0;
    float t0 = 0.0f;
    float t1 = 1.0f;
    bool in = clip_edge(-dx, *x0 + m, &t0, &t1) && clip_edge(dx, (float)RT_SCREEN_W + m - *x0, &t0, &t1) &&
              clip_edge(-dy, *y0 + m, &t0, &t1) && clip_edge(dy, (float)RT_SCREEN_H + m - *y0, &t0, &t1);
    if (!in) {
        return false;
    }
    float ax = *x0;
    float ay = *y0;
    *x0 = ax + t0 * dx;
    *y0 = ay + t0 * dy;
    *x1 = ax + t1 * dx;
    *y1 = ay + t1 * dy;
    return true;
}

void rt_canvas_thick_line(RtCanvas *cv, float x0, float y0, float x1, float y1, int32_t r, uint8_t c)
{
    if (!RN_ASSERT(cv != NULL) || !RN_ASSERT(r >= 0 && r <= 8) || !line_visible(x0, y0, x1, y1, r)) {
        return;
    }
    if (!(isfinite(x0) && isfinite(y0) && isfinite(x1) && isfinite(y1)) ||
        !clip_segment(&x0, &y0, &x1, &y1, (float)r + 2.0f)) {
        return;
    }
    int32_t ax = (int32_t)lroundf(x0), ay = (int32_t)lroundf(y0);
    int32_t bx = (int32_t)lroundf(x1), by = (int32_t)lroundf(y1);
    int32_t dx = (bx > ax) ? bx - ax : ax - bx;
    int32_t dy = -((by > ay) ? by - ay : ay - by);
    if (dx > MAX_LINE_STEPS || -dy > MAX_LINE_STEPS) {
        return;
    }
    int32_t sx = (ax < bx) ? 1 : -1;
    int32_t sy = (ay < by) ? 1 : -1;
    int32_t err = dx + dy;
    for (int32_t k = 0; k <= 2 * MAX_LINE_STEPS; k++) {        /* Bresenham, stamping discs */
        if (r == 0) {
            rt_canvas_pixel(cv, ax, ay, c);
        } else if (ax >= -r && ax < RT_SCREEN_W + r && ay >= -r && ay < RT_SCREEN_H + r) {
            rt_canvas_fill_circle(cv, ax, ay, r, c);
        }
        if (ax == bx && ay == by) {
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; ax += sx; }
        if (e2 <= dx) { err += dx; ay += sy; }
    }
}

void rt_canvas_glass(RtCanvas *cv, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (!RN_ASSERT(cv != NULL) || cv->n_glass >= (uint32_t)RT_MAX_GLASS) {
        return;
    }
    RtRect *g = &cv->glass[cv->n_glass];
    g->x = (int16_t)x;
    g->y = (int16_t)y;
    g->w = (int16_t)w;
    g->h = (int16_t)h;
    cv->n_glass++;
}
