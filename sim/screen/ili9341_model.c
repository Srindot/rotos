/*
 * ili9341_model.c - ILI9341 controller model.
 */
#include "ili9341_model.h"

#include <string.h>

void lcd_reset(LcdModel *m)
{
    (void)memset(m->gram, 0, sizeof m->gram);
    m->sleeping = true;
    m->display_on = false;
    m->xs = 0;
    m->xe = LCD_W - 1;
    m->ys = 0;
    m->ye = LCD_H - 1;
    m->cx = 0;
    m->cy = 0;
    m->hi_byte = -1;
    m->n_params = 0u;
}

void lcd_command(LcdModel *m, uint8_t c)
{
    m->cmd = c;
    m->n_params = 0u;
    m->hi_byte = -1;
    switch (c) {
    case 0x01u: lcd_reset(m); break;               /* SWRESET */
    case 0x10u: m->sleeping = true; break;         /* SLPIN   */
    case 0x11u: m->sleeping = false; break;        /* SLPOUT  */
    case 0x28u: m->display_on = false; break;      /* DISPOFF */
    case 0x29u: m->display_on = true; break;       /* DISPON  */
    case 0x2Cu: m->cx = m->xs; m->cy = m->ys; break;   /* RAMWR */
    default: break;                                /* accepted, not modelled */
    }
}

static void write_pixel(LcdModel *m, uint16_t px)
{
    if (m->cx < LCD_W && m->cy < LCD_H) {
        m->gram[m->cy * LCD_W + m->cx] = px;
    }
    m->cx++;
    if (m->cx > m->xe) {
        m->cx = m->xs;
        m->cy++;
        if (m->cy > m->ye) {
            m->cy = m->ys;
        }
    }
}

static void address(LcdModel *m)
{
    int32_t a = (m->params[0] << 8) | m->params[1];
    int32_t b = (m->params[2] << 8) | m->params[3];
    if (m->cmd == 0x2Au) {
        m->xs = a;
        m->xe = (b < LCD_W) ? b : LCD_W - 1;
    } else {
        m->ys = a;
        m->ye = (b < LCD_H) ? b : LCD_H - 1;
    }
}

void lcd_data(LcdModel *m, const uint8_t *d, uint32_t n)
{
    m->bytes_in += n;
    if (m->cmd == 0x2Cu) {
        for (uint32_t i = 0u; i < n; i++) {
            if (m->hi_byte < 0) {
                m->hi_byte = d[i];
                continue;
            }
            write_pixel(m, (uint16_t)((m->hi_byte << 8) | d[i]));   /* big-endian */
            m->hi_byte = -1;
        }
        return;
    }
    for (uint32_t i = 0u; i < n && m->n_params < 4u; i++) {
        m->params[m->n_params] = d[i];
        m->n_params++;
    }
    if (m->n_params == 4u && (m->cmd == 0x2Au || m->cmd == 0x2Bu)) {
        address(m);
    }
}

bool lcd_lit(const LcdModel *m)
{
    return m->display_on && !m->sleeping;
}
