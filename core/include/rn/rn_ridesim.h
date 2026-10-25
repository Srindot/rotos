/*
 * rn_ridesim.h - a synthetic rider for testing without a motorcycle: follows
 * the route with realistic acceleration, slows for turns, stops at a red
 * light, and produces noisy phone-GPS fixes.
 *
 * Used by the simulator's virtual phone and by the Android app's demo mode,
 * so both replay exactly the same ride for the same seed.
 */
#ifndef RN_RIDESIM_H
#define RN_RIDESIM_H

#include "rn/rn_engine.h"
#include "rn/rn_route.h"

#define RN_SIM_ACCEL        1.2f    /* m/s^2                               */
#define RN_SIM_DECEL        1.8f
#define RN_SIM_CRUISE_MPS  12.0f    /* ~43 km/h city riding                */
#define RN_SIM_FLYOVER_MPS 15.0f
#define RN_SIM_RED_LIGHT_S 12.0f
#define RN_SIM_MAX_STOPS   (RN_MAX_MANEUVERS + 1u)

typedef struct {
    float along_m;
    float speed_mps;      /* speed through it; 0 = full stop               */
    float dwell_s;        /* time standing still at a full stop            */
    bool used;
} RnSimStop;

typedef struct {
    float s;              /* metres along the route                        */
    float v;              /* m/s                                           */
    float dwell;          /* seconds left standing still                   */
    float t;              /* seconds since the ride started                */
    float cruise_mps;
    uint32_t rng;
    RnSimStop stops[RN_SIM_MAX_STOPS];
    uint32_t n_stops;
    bool done;
} RnRideSim;

void rn_ridesim_init(RnRideSim *sim, const RnRoute *r, uint32_t seed, float cruise_mps);
/* Advance dt seconds; returns false once the rider has reached the end. */
bool rn_ridesim_step(RnRideSim *sim, const RnRoute *r, float dt);

/* Noisy GPS fix of the current position. t_ms / unix_s / utc_offset_s are
 * copied into the fix as given. */
void rn_ridesim_fix(RnRideSim *sim, const RnRoute *r, float sigma_m, uint32_t t_ms,
                    double unix_s, int32_t utc_offset_s, RnFix *out);

/* Deterministic PRNG helpers (xorshift32 + Box-Muller). */
float rn_ridesim_uniform(uint32_t *state);
float rn_ridesim_gauss(uint32_t *state, float sigma);

#endif /* RN_RIDESIM_H */
