/*
 * rt_present.c - palette expansion, glass colour math, band diffing.
 */
#include "rt_present.h"

#include <string.h>

#include "hal_display.h"
#include "rn/rn_types.h"
#include "rt_ili9341.h"

#define FNV_OFFSET 2166136261u
#define FNV_PRIME  16777619u

void rt_present_init(RtPresenter *p)
{
    if (!RN_ASSERT(p != NULL)) {
        return;
    }
    (void)memset(p, 0, sizeof *p);
    p->glass_pct = RT_GLASS_DEFAULT;
    p->built_pct = -1;
}

void rt_present_set_glass(RtPresenter *p, int32_t pct)
{
    if (!RN_ASSERT(p != NULL)) {
        return;
    }
    p->glass_pct = (pct < 0) ? 0 : ((pct > 100) ? 100 : pct);
}

/* Colours 0..10 dimmed toward the panel colour; UI colours unchanged. */
static void build_glass_palette(RtPresenter *p, const RtTheme *t)
{
    uint8_t pr = 0u, pg = 0u, pb = 0u;
    rt_rgb565_split(t->rgb565[RT_C_PANEL], &pr, &pg, &pb);
    for (uint32_t i = 0u; i < (uint32_t)RT_C_COUNT; i++) {
        if (i >= (uint32_t)RT_GLASS_TINTED) {
            p->glass_pal[i] = t->rgb565[i];
            continue;
        }
        uint8_t r = 0u, g = 0u, b = 0u;
        rt_rgb565_split(t->rgb565[i], &r, &g, &b);
        int32_t k = p->glass_pct;
        r = (uint8_t)((int32_t)r + ((int32_t)pr - (int32_t)r) * k / 100);
        g = (uint8_t)((int32_t)g + ((int32_t)pg - (int32_t)g) * k / 100);
        b = (uint8_t)((int32_t)b + ((int32_t)pb - (int32_t)b) * k / 100);
        p->glass_pal[i] = rt_rgb565(r, g, b);
    }
    p->built_pct = p->glass_pct;
}

void rt_present_row(const RtPresenter *p, const RtCanvas *cv, int32_t y, uint16_t *out)
{
    if (!RN_ASSERT(p != NULL && cv != NULL && out != NULL) || !RN_ASSERT(y >= 0 && y < RT_SCREEN_H)) {
        return;
    }
    const uint16_t *pal = p->have_pal ? p->pal : RT_THEME_NIGHT.rgb565;
    const uint8_t *src = &cv->px[y * RT_STRIDE];
    for (int32_t i = 0; i < RT_STRIDE; i++) {
        out[2 * i] = pal[src[i] >> 4];
        out[2 * i + 1] = pal[src[i] & 0x0Fu];
    }
    for (uint32_t gi = 0u; gi < cv->n_glass && gi < (uint32_t)RT_MAX_GLASS; gi++) {
        const RtRect *w = &cv->glass[gi];
        if (y < w->y || y >= w->y + w->h) {
            continue;
        }
        int32_t edge = (y - w->y < w->y + w->h - 1 - y) ? y - w->y : w->y + w->h - 1 - y;
        int32_t inset = (edge == 0) ? 1 : 0;                  /* rounded corners */
        int32_t xa = (w->x + inset < 0) ? 0 : w->x + inset;
        int32_t xb = (w->x + w->w - 1 - inset >= RT_SCREEN_W) ? RT_SCREEN_W - 1 : w->x + w->w - 1 - inset;
        for (int32_t x = xa; x <= xb; x++) {
            out[x] = p->glass_pal[rt_canvas_get(cv, x, y)];
        }
    }
}

static uint32_t band_hash(const RtCanvas *cv, int32_t band)
{
    uint32_t h = FNV_OFFSET;
    const uint8_t *src = &cv->px[band * RT_BAND_H * RT_STRIDE];
    for (int32_t i = 0; i < RT_BAND_H * RT_STRIDE; i++) {
        h = (h ^ src[i]) * FNV_PRIME;
    }
    int32_t y0 = band * RT_BAND_H;
    for (uint32_t gi = 0u; gi < cv->n_glass && gi < (uint32_t)RT_MAX_GLASS; gi++) {
        const RtRect *w = &cv->glass[gi];             /* glass placement is part of the look */
        if (w->y < y0 + RT_BAND_H && w->y + w->h > y0) {
            uint32_t geo = (uint32_t)(uint16_t)w->x | ((uint32_t)(uint16_t)w->y << 9) |
                           ((uint32_t)(uint16_t)w->w << 18);
            h = ((h ^ geo) * FNV_PRIME) ^ (uint32_t)(uint16_t)w->h;
        }
    }
    return h;
}

static void send_band(RtPresenter *p, const RtCanvas *cv, int32_t band)
{
    uint8_t *o = p->line[p->which];
    p->which ^= 1u;
    uint16_t row[RT_SCREEN_W];
    int32_t y0 = band * RT_BAND_H;
    for (int32_t y = y0; y < y0 + RT_BAND_H; y++) {
        rt_present_row(p, cv, y, row);
        for (int32_t x = 0; x < RT_SCREEN_W; x++) {
            *o = (uint8_t)(row[x] >> 8);              /* big-endian on the wire */
            o++;
            *o = (uint8_t)(row[x] & 0xFFu);
            o++;
        }
    }
    rt_ili9341_window(0u, (uint16_t)y0, (uint16_t)(RT_SCREEN_W - 1), (uint16_t)(y0 + RT_BAND_H - 1));
    hal_display_data(p->line[p->which ^ 1u], (uint32_t)sizeof p->line[0]);
    p->bands_sent++;
    p->bytes_sent += (uint32_t)sizeof p->line[0];
}

void rt_present_frame(RtPresenter *p, const RtCanvas *cv, const RtTheme *t, bool force)
{
    if (!RN_ASSERT(p != NULL && cv != NULL && t != NULL)) {
        return;
    }
    bool redo = force;
    if (!p->have_pal || memcmp(p->pal, t->rgb565, sizeof p->pal) != 0 || p->built_pct != p->glass_pct) {
        redo = true;                                  /* new palette: resend all */
        (void)memcpy(p->pal, t->rgb565, sizeof p->pal);
        p->have_pal = true;
        build_glass_palette(p, t);
    }
    for (int32_t b = 0; b < RT_BANDS; b++) {
        uint32_t h = band_hash(cv, b);
        if (!redo && h == p->band_hash[b]) {
            continue;
        }
        p->band_hash[b] = h;
        send_band(p, cv, b);
    }
    hal_display_flush();
}
