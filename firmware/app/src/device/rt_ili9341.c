/*
 * rt_ili9341.c - ILI9341 bring-up and windowing.
 *
 * The init table is the panel vendor's recommended sequence (as used by
 * most 2.8" SPI modules); the screen twin accepts and ignores the power and
 * gamma commands it does not model.
 */
#include "rt_ili9341.h"

#include "hal_display.h"
#include "hal_time.h"
#include "rn/rn_types.h"

typedef struct {
    uint8_t cmd;
    uint8_t n;          /* parameter count             */
    uint8_t delay_ms;   /* wait after the command      */
    uint8_t args[15];
} InitStep;

static const InitStep INIT[] = {
    {RT_ILI_SWRESET, 0u, 150u, {0u}},
    {0xEFu, 3u, 0u, {0x03u, 0x80u, 0x02u}},
    {0xCFu, 3u, 0u, {0x00u, 0xC1u, 0x30u}},                       /* power control B   */
    {0xEDu, 4u, 0u, {0x64u, 0x03u, 0x12u, 0x81u}},                /* power on sequence */
    {0xE8u, 3u, 0u, {0x85u, 0x00u, 0x78u}},                       /* driver timing A   */
    {0xCBu, 5u, 0u, {0x39u, 0x2Cu, 0x00u, 0x34u, 0x02u}},         /* power control A   */
    {0xF7u, 1u, 0u, {0x20u}},                                     /* pump ratio        */
    {0xEAu, 2u, 0u, {0x00u, 0x00u}},                              /* driver timing B   */
    {0xC0u, 1u, 0u, {0x23u}},                                     /* power control 1   */
    {0xC1u, 1u, 0u, {0x10u}},                                     /* power control 2   */
    {0xC5u, 2u, 0u, {0x3Eu, 0x28u}},                              /* VCOM 1            */
    {0xC7u, 1u, 0u, {0x86u}},                                     /* VCOM 2            */
    {RT_ILI_COLMOD, 1u, 0u, {0x55u}},                             /* 16 bit / pixel    */
    {0xB1u, 2u, 0u, {0x00u, 0x18u}},                              /* frame rate 79 Hz  */
    {0xB6u, 3u, 0u, {0x08u, 0x82u, 0x27u}},                       /* display function  */
    {0xF2u, 1u, 0u, {0x00u}},                                     /* 3-gamma off       */
    {0x26u, 1u, 0u, {0x01u}},                                     /* gamma curve 1     */
    {0xE0u, 15u, 0u, {0x0Fu, 0x31u, 0x2Bu, 0x0Cu, 0x0Eu, 0x08u, 0x4Eu, 0xF1u,
                      0x37u, 0x07u, 0x10u, 0x03u, 0x0Eu, 0x09u, 0x00u}},
    {0xE1u, 15u, 0u, {0x00u, 0x0Eu, 0x14u, 0x03u, 0x11u, 0x07u, 0x31u, 0xC1u,
                      0x48u, 0x08u, 0x0Fu, 0x0Cu, 0x31u, 0x36u, 0x0Fu}},
    {RT_ILI_SLPOUT, 0u, 120u, {0u}},
    {RT_ILI_DISPON, 0u, 20u, {0u}},
};

/* Send INIT[first..]; quick = refresh: short waits only (see rt_ili9341_refresh). */
static void send_table(uint32_t first, bool quick)
{
    uint32_t n = (uint32_t)(sizeof INIT / sizeof INIT[0]);
    for (uint32_t i = first; i < n; i++) {
        const InitStep *s = &INIT[i];
        (void)RN_ASSERT(s->n <= sizeof s->args);
        hal_display_command(s->cmd);
        if (s->n > 0u) {
            hal_display_data(s->args, s->n);
        }
        hal_display_flush();
        uint32_t wait = (quick && s->delay_ms > 0u) ? RT_ILI_QUICK_WAIT_MS : (uint32_t)s->delay_ms;
        if (wait > 0u) {
            hal_time_sleep_until(hal_time_ms() + wait);
        }
    }
    uint8_t madctl = hal_display_madctl();             /* orientation is per board */
    hal_display_command(RT_ILI_MADCTL);
    hal_display_data(&madctl, 1u);
    hal_display_flush();
}

void rt_ili9341_init(void)
{
    hal_display_reset();
    send_table(0u, false);
}

void rt_ili9341_refresh(void)
{
    send_table(1u, true);                               /* everything but the software reset */
}

void rt_ili9341_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    (void)RN_ASSERT(x0 <= x1 && y0 <= y1);
    uint8_t ca[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t pa[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    hal_display_command(RT_ILI_CASET);
    hal_display_data(ca, 4u);
    hal_display_command(RT_ILI_PASET);
    hal_display_data(pa, 4u);
    hal_display_command(RT_ILI_RAMWR);
}
