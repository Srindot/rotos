/*
 * rt_themectl.c - automatic day / night with hold and fade.
 */
#include "rt_themectl.h"

#include <string.h>

#include "rn/rn_protocol.h"
#include "rn/rn_types.h"
#include "rt_config.h"

void rt_themectl_init(RtThemeCtl *c, uint8_t mode)
{
    if (!RN_ASSERT(c != NULL)) {
        return;
    }
    (void)memset(c, 0, sizeof *c);
    c->mode = (mode <= RN_THEME_NIGHT) ? mode : RN_THEME_AUTO;
    c->night = true;
    c->pending = true;
    c->blended = RT_THEME_NIGHT;
}

void rt_themectl_set_mode(RtThemeCtl *c, uint8_t mode)
{
    if (!RN_ASSERT(c != NULL) || !RN_ASSERT(mode <= RN_THEME_NIGHT)) {
        return;
    }
    c->mode = mode;
}

static bool wanted(const RtThemeCtl *c, bool phone_known, bool phone_night)
{
    if (c->mode == RN_THEME_DAY) {
        return false;
    }
    if (c->mode == RN_THEME_NIGHT) {
        return true;
    }
    return phone_known ? phone_night : c->night;
}

void rt_themectl_update(RtThemeCtl *c, bool phone_known, bool phone_night, uint32_t now_ms)
{
    if (!RN_ASSERT(c != NULL)) {
        return;
    }
    bool want = wanted(c, phone_known, phone_night);
    if (want != c->pending) {
        c->pending = want;
        c->pending_since_ms = now_ms;
    }
    bool hold = c->mode == RN_THEME_AUTO && c->decided_once;
    if (c->mode == RN_THEME_AUTO && phone_known) {
        c->decided_once = true;
    }
    if (c->pending != c->night && (!hold || now_ms - c->pending_since_ms >= RT_THEME_HOLD_MS)) {
        c->night = c->pending;
    }
    float target = c->night ? 1.0f : 0.0f;
    if (!c->fade_init) {
        c->fade = target;
        c->fade_init = true;
    }
    float step = (float)(now_ms - c->last_ms) / (float)RT_THEME_FADE_MS;
    c->last_ms = now_ms;
    step = (step > 1.0f) ? 1.0f : step;
    c->fade = (c->fade < target) ? ((c->fade + step > target) ? target : c->fade + step)
                                 : ((c->fade - step < target) ? target : c->fade - step);
    rt_theme_blend(&RT_THEME_DAY, &RT_THEME_NIGHT, (uint8_t)(c->fade * 255.0f + 0.5f), &c->blended);
}

const RtTheme *rt_themectl_theme(const RtThemeCtl *c)
{
    if (!RN_ASSERT(c != NULL)) {
        return &RT_THEME_NIGHT;
    }
    return &c->blended;
}
