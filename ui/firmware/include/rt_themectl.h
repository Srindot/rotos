/*
 * rt_themectl.h - decides day or night and crossfades the palettes.
 *
 * AUTO follows the phone's NIGHT flag (sun below the horizon at the rider's
 * position). A flip must hold for 3 s before it happens (no flicker), then
 * the 16 palette entries fade over 1 s, NES style. Manual modes and the
 * phone's first answer apply at once.
 */
#ifndef RT_THEMECTL_H
#define RT_THEMECTL_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_theme.h"

typedef struct {
    uint8_t mode;             /* RN_THEME_AUTO / DAY / NIGHT        */
    bool night;               /* shown (or fading to)              */
    bool pending;             /* requested                         */
    uint32_t pending_since_ms;
    bool decided_once;        /* first phone decision applies now  */
    bool fade_init;
    float fade;               /* 0 = day palette, 1 = night        */
    uint32_t last_ms;
    RtTheme blended;
} RtThemeCtl;

void rt_themectl_init(RtThemeCtl *c, uint8_t mode);
void rt_themectl_set_mode(RtThemeCtl *c, uint8_t mode);
/* phone_known: the phone has told us day or night at least once. */
void rt_themectl_update(RtThemeCtl *c, bool phone_known, bool phone_night, uint32_t now_ms);
const RtTheme *rt_themectl_theme(const RtThemeCtl *c);

#endif /* RT_THEMECTL_H */
