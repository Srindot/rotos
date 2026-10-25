/*
 * ht_buttons.h - button state for the hardware test.
 *
 * Reads the raw pins every frame (no debounce, on purpose: a bouncing or
 * stuck button should be visible), counts presses and logs each edge.
 */
#ifndef HT_BUTTONS_H
#define HT_BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_input.h"

typedef struct {
    uint8_t held;                         /* bit i = button i down now */
    uint32_t presses[HAL_BUTTON_COUNT];
} HtButtons;

void ht_buttons_init(HtButtons *b);
void ht_buttons_poll(HtButtons *b);
bool ht_buttons_down(const HtButtons *b, uint32_t i);

#endif /* HT_BUTTONS_H */
