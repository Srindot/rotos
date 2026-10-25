/*
 * rt_input.h - button debouncing and short / long press detection.
 *
 *   A short: next page (map -> dashboard -> weather); dismiss the summary card
 *   while a call rings: A = reject + SMS reply, B = ignore (rt_call)
 *   A long : north-up <-> heading-up
 *   B short: theme AUTO -> DAY -> NIGHT
 *   B long : glass opacity 50 / 70 / 90 %
 *   C      : sent to the phone (reserved for the call feature)
 * Every event is also forwarded to the phone as a BUTTON packet.
 */
#ifndef RT_INPUT_H
#define RT_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "hal_input.h"

typedef struct {
    uint8_t button;     /* 0..2                     */
    uint8_t event;      /* RN_BUTTON_SHORT / _LONG   */
} RtButtonEvent;

typedef struct {
    uint8_t stable;                          /* debounced mask   */
    uint8_t raw;                             /* last raw mask    */
    uint32_t raw_since[HAL_BUTTON_COUNT];
    uint32_t press_ms[HAL_BUTTON_COUNT];
    bool long_fired[HAL_BUTTON_COUNT];
} RtInput;

void rt_input_init(RtInput *in);
/* Feed the raw mask; returns the number of events written (<= cap). */
uint32_t rt_input_poll(RtInput *in, uint8_t raw, uint32_t now_ms, RtButtonEvent *events, uint32_t cap);

#endif /* RT_INPUT_H */
