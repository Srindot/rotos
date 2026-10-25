/*
 * hal_time_sim.c - wall clock (scaled by --speed) or a virtual clock in
 * headless mode, so headless runs are deterministic and fast.
 */
#include <time.h>

#include "hal_time.h"
#include "sim_config.h"

static uint64_t s_virtual_ms;
static uint64_t s_t0_ns;

/* Virtual time only makes sense when nothing outside drives the clock: a
 * live TCP phone runs in real time even without a window. */
static bool virtual_clock(const SimConfig *c)
{
    return c->headless && c->link_mode != SIM_LINK_TCP;
}

static uint64_t mono_ns(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

void sim_time_advance(uint32_t ms)
{
    s_virtual_ms += ms;
}

uint32_t hal_time_ms(void)
{
    const SimConfig *c = sim_config();
    if (virtual_clock(c)) {
        return (uint32_t)s_virtual_ms;
    }
    if (s_t0_ns == 0u) {
        s_t0_ns = mono_ns();
    }
    double ms = (double)(mono_ns() - s_t0_ns) / 1e6 * (double)c->speed;
    return (uint32_t)(uint64_t)ms;
}

void hal_time_sleep_until(uint32_t deadline_ms)
{
    const SimConfig *c = sim_config();
    if (virtual_clock(c)) {
        if ((int32_t)(deadline_ms - (uint32_t)s_virtual_ms) > 0) {
            s_virtual_ms = deadline_ms;
        }
        return;
    }
    for (int k = 0; k < 1000; k++) {
        int32_t left = (int32_t)(deadline_ms - hal_time_ms());
        if (left <= 0) {
            return;
        }
        double real_ms = (double)left / (double)c->speed;
        struct timespec ts = {0, (long)(real_ms * 1e6)};
        if (real_ms >= 1000.0) {
            ts.tv_sec = 1;
            ts.tv_nsec = 0;
        }
        (void)nanosleep(&ts, NULL);
    }
}
