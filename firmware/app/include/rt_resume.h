/*
 * rt_resume.h - the ride in progress, kept in flash so a power cut (engine
 * switched off at a signal) does not end it. ROTOS has no clock of its own:
 * the snapshot carries the phone's local time and date of its last save, and
 * when the phone is back its time says how long ROTOS was off. Up to
 * RT_RESUME_MAX_S the ride simply goes on; longer, it is closed.
 */
#ifndef RT_RESUME_H
#define RT_RESUME_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_protocol.h"
#include "rt_ride.h"

#define RT_RESUME_VERSION 1u
#define RT_RESUME_OFF_UNKNOWN 0xFFFFFFFFu

typedef struct {
    uint8_t version;
    uint8_t have_clock;       /* the ride's start time is known            */
    uint8_t saved_day;        /* phone date at the save                    */
    uint8_t saved_month;
    uint32_t saved_s;         /* phone local seconds of day at the save    */
    uint32_t start_s;
    uint32_t ride_ms;         /* ridden so far                             */
    uint32_t moving_ms;
    float distance_m;
    float max_mps;
    RnNavState nav;           /* the last position: the map shows it at boot */
} RtResumeSnap;

/* The snapshot of [ride] (ridden until now_ms), phone time saved_s on day/month. */
RtResumeSnap rt_resume_snap(const RtRide *ride, const RnNavState *nav, uint32_t now_ms,
                            uint32_t saved_s, uint8_t day, uint8_t month);
bool rt_resume_save(const RtResumeSnap *s);
bool rt_resume_load(RtResumeSnap *out);
void rt_resume_clear(void);
/* Seconds between the save and now (phone time and date), or RT_RESUME_OFF_UNKNOWN
 * when that cannot be told (another day, or the clock went back). Pure. */
uint32_t rt_resume_off_s(const RtResumeSnap *s, uint32_t now_s, uint8_t day, uint8_t month);

#endif /* RT_RESUME_H */
