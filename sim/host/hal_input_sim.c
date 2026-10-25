/*
 * hal_input_sim.c - buttons and light sensor from the screen twin's keys,
 * plus scripted presses (--press A@20000) for headless runs.
 */
#include "hal_input.h"
#include "hal_time.h"
#include "sim_config.h"

bool hal_input_init(void)
{
    return true;
}

static uint8_t scripted(uint32_t now_ms)
{
    const SimConfig *c = sim_config();
    uint8_t mask = 0u;
    for (uint32_t i = 0u; i < c->n_presses && i < SIM_MAX_PRESSES; i++) {
        const SimPress *p = &c->presses[i];
        if (now_ms >= p->at_ms && now_ms < p->at_ms + p->hold_ms) {
            uint32_t bit = (p->button == 'A') ? 0u : ((p->button == 'B') ? 1u : 2u);
            mask |= (uint8_t)(1u << bit);
        }
    }
    return mask;
}

uint8_t hal_input_buttons(void)
{
    sim_display_poll();
    return (uint8_t)(sim_display_buttons() | scripted(hal_time_ms()));
}

int32_t hal_input_light(void)
{
    return sim_display_light();
}
