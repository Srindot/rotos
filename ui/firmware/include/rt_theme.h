/*
 * rt_theme.h - 16-colour palettes and the day/night crossfade.
 */
#ifndef RT_THEME_H
#define RT_THEME_H

#include <stdint.h>

#include "rt_colors.h"

typedef enum {
    RT_PANEL_GRADIENT = 0,   /* Hyprland: 1 px border dithered ACCENT -> HILITE */
    RT_PANEL_DOUBLE = 1      /* Pokemon dialog box: dark outer + accent inner   */
} RtPanelStyle;

typedef struct {
    uint16_t rgb565[RT_C_COUNT];
    uint8_t panel;           /* RtPanelStyle */
} RtTheme;

extern const RtTheme RT_THEME_NIGHT;
extern const RtTheme RT_THEME_DAY;

/* NES-style palette fade: t = 0 -> a, 255 -> b. Panel style flips halfway. */
void rt_theme_blend(const RtTheme *a, const RtTheme *b, uint8_t t, RtTheme *out);
/* 8-bit channels of an RGB565 colour. */
void rt_rgb565_split(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b);
uint16_t rt_rgb565(uint8_t r, uint8_t g, uint8_t b);

#endif /* RT_THEME_H */
