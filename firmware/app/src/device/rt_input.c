/*
 * rt_input.c - debouncing and press classification.
 */
#include "rt_input.h"

#include <string.h>

#include "rn/rn_protocol.h"
#include "rn/rn_types.h"
#include "rt_config.h"

void rt_input_init(RtInput *in)
{
    if (!RN_ASSERT(in != NULL)) {
        return;
    }
    (void)memset(in, 0, sizeof *in);
}

static void push(RtButtonEvent *events, uint32_t cap, uint32_t *n, uint8_t button, uint8_t event)
{
    if (*n < cap) {
        events[*n].button = button;
        events[*n].event = event;
        *n += 1u;
    }
}

uint32_t rt_input_poll(RtInput *in, uint8_t raw, uint32_t now_ms, RtButtonEvent *events, uint32_t cap)
{
    if (!RN_ASSERT(in != NULL && events != NULL)) {
        return 0u;
    }
    uint32_t n = 0u;
    for (uint8_t b = 0u; b < (uint8_t)HAL_BUTTON_COUNT; b++) {
        uint8_t bit = (uint8_t)(1u << b);
        if ((raw & bit) != (in->raw & bit)) {
            in->raw_since[b] = now_ms;              /* raw edge: restart debounce */
        }
        bool raw_down = (raw & bit) != 0u;
        bool was_down = (in->stable & bit) != 0u;
        if (raw_down != was_down && now_ms - in->raw_since[b] >= RT_DEBOUNCE_MS) {
            in->stable ^= bit;
            if (raw_down) {
                in->press_ms[b] = now_ms;
                in->long_fired[b] = false;
            } else if (!in->long_fired[b]) {
                push(events, cap, &n, b, RN_BUTTON_SHORT);
            } else {
                /* release after a long press: already reported */
            }
        }
        if ((in->stable & bit) != 0u && !in->long_fired[b] && now_ms - in->press_ms[b] >= RT_LONG_PRESS_MS) {
            in->long_fired[b] = true;
            push(events, cap, &n, b, RN_BUTTON_LONG);
        }
    }
    in->raw = raw;
    return n;
}
