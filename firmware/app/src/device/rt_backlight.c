/*
 * rt_backlight.c - automatic brightness.
 */
#include "rt_backlight.h"

#include <math.h>
#include <string.h>

#include "rn/rn_types.h"

#define TAU_MS     2000.0f
#define MIN_PCT      20.0f
#define NIGHT_CAP    70.0f

void rt_backlight_init(RtBacklight *b)
{
    if (!RN_ASSERT(b != NULL)) {
        return;
    }
    (void)memset(b, 0, sizeof *b);
}

static float target_pct(int32_t light, uint8_t setting_pct, bool night)
{
    if (setting_pct > 0u) {
        return (float)setting_pct;
    }
    if (light < 0) {
        return night ? 60.0f : 100.0f;              /* no sensor fitted */
    }
    float t = MIN_PCT + (float)light * (100.0f - MIN_PCT) / 100.0f;
    return night ? fminf(t, NIGHT_CAP) : t;
}

uint8_t rt_backlight_update(RtBacklight *b, int32_t light, uint8_t setting_pct, bool night, uint32_t now_ms)
{
    if (!RN_ASSERT(b != NULL) || !RN_ASSERT(setting_pct <= 100u)) {
        return 100u;
    }
    float target = target_pct((light > 100) ? 100 : light, setting_pct, night);
    if (!b->init) {
        b->level = target;
        b->init = true;
    } else {
        float dt = (float)(now_ms - b->last_ms);
        b->level += (target - b->level) * (1.0f - expf(-dt / TAU_MS));
    }
    b->last_ms = now_ms;
    float lv = fminf(fmaxf(b->level, MIN_PCT), 100.0f);
    b->applied = (uint8_t)(lv + 0.5f);
    return b->applied;
}
