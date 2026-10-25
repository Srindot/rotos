/*
 * ht_buttons.c - press counting and serial log for the hardware test.
 */
#include "ht_buttons.h"

#include <string.h>

#include "board.h"
#include "hal_log.h"
#include "rn/rn_types.h"

static const char *const DOWN_MSG[HAL_BUTTON_COUNT] = {"button A pressed, GPIO", "button B pressed, GPIO",
                                                       "button C pressed, GPIO"};
static const char *const UP_MSG[HAL_BUTTON_COUNT] = {"button A released, GPIO", "button B released, GPIO",
                                                     "button C released, GPIO"};
static const int32_t PIN[HAL_BUTTON_COUNT] = {BOARD_PIN_BTN_A, BOARD_PIN_BTN_B, BOARD_PIN_BTN_C};

void ht_buttons_init(HtButtons *b)
{
    if (!RN_ASSERT(b != NULL)) {
        return;
    }
    (void)memset(b, 0, sizeof *b);
    b->held = hal_input_buttons();
    if (b->held != 0u) {
        hal_log("btn", "held at start (stuck or wired to the wrong leg?), mask", b->held);
    }
}

void ht_buttons_poll(HtButtons *b)
{
    if (!RN_ASSERT(b != NULL)) {
        return;
    }
    uint8_t now = hal_input_buttons();
    uint8_t changed = (uint8_t)(now ^ b->held);
    for (uint32_t i = 0u; i < HAL_BUTTON_COUNT; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        if ((changed & bit) == 0u) {
            continue;
        }
        if ((now & bit) != 0u) {
            b->presses[i]++;
            hal_log("btn", DOWN_MSG[i], PIN[i]);
        } else {
            hal_log("btn", UP_MSG[i], PIN[i]);
        }
    }
    b->held = now;
}

bool ht_buttons_down(const HtButtons *b, uint32_t i)
{
    if (!RN_ASSERT(b != NULL && i < HAL_BUTTON_COUNT)) {
        return false;
    }
    return (b->held & (1u << i)) != 0u;
}
