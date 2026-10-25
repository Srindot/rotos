/*
 * rt_call.h - the incoming-call popup (docs/PROTOCOL.md, "Calls").
 *
 *   HIDDEN --CALL_STATE ringing--> RINGING --A--> SENDING --phone: replied--> DONE (2 s) --> HIDDEN
 *                                     |  B --> HIDDEN (this call stays dismissed)
 *                                     +-- answered / ended / 10 s silence --> HIDDEN
 *
 * Pure state machine: it decides what to show and which CALL_ACTION to send;
 * rt_app sends it and rt_screen_call draws it.
 */
#ifndef RT_CALL_H
#define RT_CALL_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_protocol.h"

#define RT_CALL_SILENCE_MS 10000u   /* no CALL_STATE: assume it is over      */
#define RT_CALL_DONE_MS     2000u   /* "REPLY SENT" stays this long          */

typedef enum {
    RT_CALL_HIDDEN = 0,
    RT_CALL_RINGING,
    RT_CALL_SENDING,
    RT_CALL_DONE
} RtCallUi;

typedef struct {
    uint8_t ui;              /* RtCallUi                                  */
    RnCallState call;
    uint32_t rx_ms;          /* last CALL_STATE                           */
    uint32_t since_ms;       /* entered the current ui state              */
    bool dismissed;
    uint16_t dismissed_id;
} RtCall;

void rt_call_init(RtCall *c);
void rt_call_on_state(RtCall *c, const RnCallState *s, uint32_t now_ms);
void rt_call_update(RtCall *c, uint32_t now_ms);
/* Button press while the popup is up: returns the CALL_ACTION to send
 * (RN_ACTION_*), or 0 when the press is not for the popup. */
uint8_t rt_call_press(RtCall *c, uint8_t button, uint32_t now_ms);
/* The popup is on screen (buttons A/B belong to it while ringing). */
bool rt_call_visible(const RtCall *c);
bool rt_call_owns_buttons(const RtCall *c);

#endif /* RT_CALL_H */
