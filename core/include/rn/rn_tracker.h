/*
 * rn_tracker.h - device-side smoothing. Turns 1 Hz, slightly late, sometimes
 * lost NAV_STATE packets into smooth 30 fps motion along the route:
 * dead-reckon with the last speed (never more than 2 s), ease toward the
 * prediction, never jitter backwards, and take the heading from the route
 * itself (steadier than GPS course).
 */
#ifndef RN_TRACKER_H
#define RN_TRACKER_H

#include "rn/rn_mappack.h"
#include "rn/rn_protocol.h"

#define RN_TRACK_MAX_PREDICT_S  2.0f
#define RN_TRACK_JUMP_M        60.0f   /* bigger corrections snap (reroute) */
#define RN_TRACK_BACK_M        15.0f   /* smaller backward ones are ignored */
#define RN_TRACK_FREE_TAU_S     0.25f  /* free mode position easing (snappy) */
#define RN_TRACK_TURN_MPS       1.0f   /* free mode: heading only when moving */
#define RN_TRACK_ALONG_TAU_S    0.3f
#define RN_TRACK_SPEED_TAU_S    0.3f
#define RN_TRACK_HEADING_TAU_S  0.35f

typedef struct {
    RnNavState last;      /* latest packet                                 */
    uint32_t rx_ms;       /* device time it arrived                        */
    uint32_t prev_ms;     /* device time of the previous update            */
    bool have;            /* any packet yet                                */
    bool init;            /* smoothing initialised                         */
    float along_m;        /* smoothed outputs                              */
    float x_px;           /* RN_NAV_FREE: position in pack pixels          */
    float y_px;
    bool free;            /* latest packet is RN_NAV_FREE                  */
    float odo_m;          /* free mode: distance travelled (smoothed)      */
    float heading_rad;    /* 0 = north, clockwise                          */
    float speed_mps;
} RnTracker;

void rn_tracker_reset(RnTracker *t);
void rn_tracker_on_nav(RnTracker *t, const RnNavState *s, uint32_t now_ms);
void rn_tracker_update(RnTracker *t, const RnPack *pack, uint32_t now_ms);
/* Milliseconds since the last packet; UINT32_MAX when none. */
uint32_t rn_tracker_age_ms(const RnTracker *t, uint32_t now_ms);

#endif /* RN_TRACKER_H */
