/*
 * ili9341_model.h - behavioural model of the ILI9341 controller: command
 * decoding, column/page address windows, GRAM, sleep and display on/off.
 * Knows nothing about ROTOS.
 */
#ifndef ILI9341_MODEL_H
#define ILI9341_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define LCD_W 240
#define LCD_H 320

typedef struct {
    uint16_t gram[LCD_W * LCD_H];   /* RGB565 as stored by the panel */
    bool sleeping;
    bool display_on;
    uint8_t cmd;
    uint8_t params[4];
    uint32_t n_params;
    int32_t xs, xe, ys, ye, cx, cy;
    int32_t hi_byte;                /* -1 = expecting the high byte  */
    uint64_t bytes_in;
    uint8_t backlight;              /* % */
} LcdModel;

void lcd_reset(LcdModel *m);
void lcd_command(LcdModel *m, uint8_t c);
void lcd_data(LcdModel *m, const uint8_t *d, uint32_t n);
bool lcd_lit(const LcdModel *m);

#endif /* ILI9341_MODEL_H */
