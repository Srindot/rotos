/*
 * rt_call.c - incoming-call popup state machine.
 */
#include "rt_call.h"

#include <string.h>

#include "rn/rn_types.h"

#define BUTTON_A 0u
#define BUTTON_B 1u

void rt_call_init(RtCall *c)
{
    if (!RN_ASSERT(c != NULL)) {
        return;
    }
    (void)memset(c, 0, sizeof *c);
}

static void enter(RtCall *c, uint8_t ui, uint32_t now_ms)
{
    c->ui = ui;
    c->since_ms = now_ms;
}

static void on_ringing(RtCall *c, const RnCallState *s, uint32_t now_ms)
{
    bool same = c->ui != (uint8_t)RT_CALL_HIDDEN && c->call.call_id == s->call_id;
    if (c->dismissed && c->dismissed_id == s->call_id) {
        return;                                  /* rider said "ignore" */
    }
    if (!same) {
        enter(c, RT_CALL_RINGING, now_ms);
    }
    c->call = *s;
}

void rt_call_on_state(RtCall *c, const RnCallState *s, uint32_t now_ms)
{
    if (!RN_ASSERT(c != NULL && s != NULL) || !RN_ASSERT(s->state <= RN_CALL_ACTIVE)) {
        return;
    }
    c->rx_ms = now_ms;
    if (s->state == RN_CALL_RINGING) {
        on_ringing(c, s, now_ms);
        return;
    }
    bool ours = c->ui != (uint8_t)RT_CALL_HIDDEN && c->call.call_id == s->call_id;
    bool replied = (s->flags & RN_CALL_F_REPLIED) != 0u;
    if (ours && replied && s->state == RN_CALL_IDLE) {
        c->call = *s;
        enter(c, RT_CALL_DONE, now_ms);
    } else if (ours || s->state == RN_CALL_ACTIVE) {
        enter(c, RT_CALL_HIDDEN, now_ms);       /* answered, or ended by the caller */
    } else {
        /* an old call ending: nothing on screen to change */
    }
}

void rt_call_update(RtCall *c, uint32_t now_ms)
{
    if (!RN_ASSERT(c != NULL)) {
        return;
    }
    bool waiting = c->ui == (uint8_t)RT_CALL_RINGING || c->ui == (uint8_t)RT_CALL_SENDING;
    if (waiting && now_ms - c->rx_ms > RT_CALL_SILENCE_MS) {
        enter(c, RT_CALL_HIDDEN, now_ms);
    }
    if (c->ui == (uint8_t)RT_CALL_DONE && now_ms - c->since_ms > RT_CALL_DONE_MS) {
        enter(c, RT_CALL_HIDDEN, now_ms);
    }
}

uint8_t rt_call_press(RtCall *c, uint8_t button, uint32_t now_ms)
{
    if (!RN_ASSERT(c != NULL) || c->ui != (uint8_t)RT_CALL_RINGING) {
        return 0u;
    }
    if (button == BUTTON_A) {
        enter(c, RT_CALL_SENDING, now_ms);
        c->rx_ms = now_ms;                       /* give the phone time to answer */
        return RN_ACTION_REJECT_REPLY;
    }
    if (button == BUTTON_B) {
        c->dismissed = true;
        c->dismissed_id = c->call.call_id;
        enter(c, RT_CALL_HIDDEN, now_ms);
        return RN_ACTION_DISMISS;
    }
    return 0u;
}

bool rt_call_visible(const RtCall *c)
{
    return RN_ASSERT(c != NULL) && c->ui != (uint8_t)RT_CALL_HIDDEN;
}

bool rt_call_owns_buttons(const RtCall *c)
{
    return RN_ASSERT(c != NULL) && c->ui == (uint8_t)RT_CALL_RINGING;
}
