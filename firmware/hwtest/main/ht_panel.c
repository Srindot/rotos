/*
 * ht_panel.c - orientation modes and the turned (landscape) frame push.
 */
#include "ht_panel.h"

#include "hal_display.h"
#include "rn/rn_types.h"
#include "rt_ili9341.h"

#define FILL_ROWS   12               /* 12 x 320 x 2 = 7680 bytes per transfer */

/* MY MX MV ML BGR. This module's controller is 320 columns x 240 rows, so
 * the row/column exchange (MV) modes come first: they give the 240 x 320
 * portrait address space ROTOS draws in. Then the four non-exchanged ones. */
static const uint8_t MADCTL[HT_MODE_COUNT] = {0x28u, 0x68u, 0xA8u, 0xE8u, 0x48u, 0x88u, 0x08u, 0xC8u};
static uint8_t s_line[2][320 * FILL_ROWS * 2];
static uint32_t s_which;

uint8_t ht_panel_madctl(uint32_t mode)
{
    if (!RN_ASSERT(mode < HT_MODE_COUNT)) {
        return MADCTL[0];
    }
    return MADCTL[mode];
}

uint32_t ht_panel_mode_of(uint8_t madctl)
{
    for (uint32_t i = 0u; i < HT_MODE_COUNT; i++) {
        if (MADCTL[i] == madctl) {
            return i;
        }
    }
    return 0u;
}

void ht_panel_set_mode(uint32_t mode)
{
    uint8_t v = ht_panel_madctl(mode);
    hal_display_command(RT_ILI_MADCTL);
    hal_display_data(&v, 1u);
    hal_display_flush();
}

#define FILL_W 320
#define FILL_H 480

void ht_panel_fill(uint16_t rgb565)
{
    ht_panel_set_mode(0u);
    for (uint32_t b = 0u; b < 2u; b++) {
        for (uint32_t i = 0u; i < (uint32_t)sizeof s_line[0]; i += 2u) {
            s_line[b][i] = (uint8_t)(rgb565 >> 8);
            s_line[b][i + 1u] = (uint8_t)(rgb565 & 0xFFu);
        }
    }
    rt_ili9341_window(0u, 0u, (uint16_t)(FILL_W - 1), (uint16_t)(FILL_H - 1));
    uint32_t total = (uint32_t)FILL_W * (uint32_t)FILL_H * 2u;
    for (uint32_t sent = 0u; sent < total; sent += (uint32_t)sizeof s_line[0]) {
        hal_display_data(s_line[s_which], (uint32_t)sizeof s_line[0]);
        s_which ^= 1u;
    }
    hal_display_flush();
}

#define RULER_CELL 40

static const uint16_t RULER_COLOR[FILL_W / RULER_CELL] = {0xFFFFu, 0xF800u, 0xFC00u, 0xFFE0u,
                                                         0x07E0u, 0x07FFu, 0x001Fu, 0xF81Fu};

static void ruler_rows(int32_t y0, uint8_t *o)
{
    for (int32_t y = y0; y < y0 + FILL_ROWS; y++) {
        bool dark = ((y / RULER_CELL) % 2) == 1;
        for (int32_t x = 0; x < FILL_W; x++) {
            uint16_t px = dark ? 0x0000u : RULER_COLOR[x / RULER_CELL];
            *o = (uint8_t)(px >> 8);
            o++;
            *o = (uint8_t)(px & 0xFFu);
            o++;
        }
    }
}

void ht_panel_ruler(void)
{
    ht_panel_set_mode(0u);
    rt_ili9341_window(0u, 0u, (uint16_t)(FILL_W - 1), (uint16_t)(FILL_H - 1));
    for (int32_t y0 = 0; y0 < FILL_H; y0 += FILL_ROWS) {
        uint8_t *o = s_line[s_which];
        s_which ^= 1u;
        ruler_rows(y0, o);
        hal_display_data(o, (uint32_t)sizeof s_line[0]);
    }
    hal_display_flush();
}

void ht_panel_push(RtPresenter *p, const RtCanvas *cv, const RtTheme *t, uint32_t mode, bool force)
{
    if (!RN_ASSERT(p != NULL && cv != NULL && t != NULL)) {
        return;
    }
    (void)mode;
    rt_present_frame(p, cv, t, force);
}
