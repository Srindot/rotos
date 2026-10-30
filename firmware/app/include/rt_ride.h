/*
 * rt_ride.h - ride statistics for the summary card. Distance is accumulated
 * from forward progress along the route, so a reroute (new map, along
 * restarting at 0) does not lose what was already ridden.
 */
#ifndef RT_RIDE_H
#define RT_RIDE_H

#include <stdbool.h>
#include <stdint.h>

#include "rt_screens.h"

typedef struct {
    bool active;
    uint32_t start_ms;
    uint32_t moving_ms;
    float distance_m;
    float max_mps;
    float last_along;
    uint32_t last_ms;
    bool have_last;
    bool have_clock;
    uint32_t start_s;
} RtRide;

void rt_ride_start(RtRide *r, uint32_t now_ms, bool have_clock, uint32_t clock_s);
/* Per frame while navigating: smoothed along and speed. */
void rt_ride_update(RtRide *r, float along_m, float speed_mps, uint32_t now_ms);
/* Forget along history (new route). */
void rt_ride_rebase(RtRide *r);
/* Carry on a ride saved before a power cut: ride_ms already ridden. */
void rt_ride_restore(RtRide *r, uint32_t now_ms, float distance_m, uint32_t ride_ms, uint32_t moving_ms,
                     float max_mps, bool have_clock, uint32_t start_s);
RtRideSummary rt_ride_finish(RtRide *r, uint32_t now_ms, bool have_clock, uint32_t clock_s);

#endif /* RT_RIDE_H */
