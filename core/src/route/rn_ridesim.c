/*
 * rn_ridesim.c - synthetic rider.
 */
#include "rn/rn_ridesim.h"

#include <math.h>
#include <string.h>

#include "rn/rn_text.h"

#define STOP_SNAP_M 0.8f

float rn_ridesim_uniform(uint32_t *state)
{
    if (!RN_ASSERT(state != NULL)) {
        return 0.5f;
    }
    uint32_t x = (*state != 0u) ? *state : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return ((float)(x >> 8) + 0.5f) / 16777216.0f;    /* (0, 1) */
}

float rn_ridesim_gauss(uint32_t *state, float sigma)
{
    float u1 = rn_ridesim_uniform(state);
    float u2 = rn_ridesim_uniform(state);
    (void)RN_ASSERT(u1 > 0.0f);
    return sigma * sqrtf(-2.0f * logf(u1)) * cosf(2.0f * RN_PI_F * u2);
}

static void add_stop(RnRideSim *sim, float along, float speed, float dwell)
{
    if (sim->n_stops >= RN_SIM_MAX_STOPS) {
        return;
    }
    uint32_t at = sim->n_stops;
    while (at > 0u && sim->stops[at - 1u].along_m > along) {    /* keep sorted */
        sim->stops[at] = sim->stops[at - 1u];
        at--;
    }
    sim->stops[at].along_m = along;
    sim->stops[at].speed_mps = speed;
    sim->stops[at].dwell_s = dwell;
    sim->stops[at].used = false;
    sim->n_stops++;
}

void rn_ridesim_init(RnRideSim *sim, const RnRoute *r, uint32_t seed, float cruise_mps)
{
    if (!RN_ASSERT(sim != NULL && r != NULL) || !RN_ASSERT(cruise_mps > 0.0f)) {
        return;
    }
    (void)memset(sim, 0, sizeof *sim);
    sim->cruise_mps = cruise_mps;
    sim->rng = (seed != 0u) ? seed : 1u;
    for (uint32_t i = 0u; i < r->n_man; i++) {
        float vt = rn_maneuver_turn_speed(r->man[i].type);
        if (vt >= 0.0f) {
            add_stop(sim, r->man[i].along_m, vt, 0.0f);
        }
    }
    add_stop(sim, r->total_m * 0.33f, 0.0f, RN_SIM_RED_LIGHT_S);
}

static float cruise_at(const RnRideSim *sim, const RnRoute *r)
{
    const char *road = "";
    for (uint32_t i = 0u; i < r->n_man; i++) {
        if (r->man[i].along_m <= sim->s) {
            road = r->man[i].road;
        }
    }
    return rn_text_contains_ci(road, "FLYOVER") ? RN_SIM_FLYOVER_MPS : sim->cruise_mps;
}

/* Speed limit from braking distance to every stop ahead. */
static float speed_limit(const RnRideSim *sim, const RnRoute *r)
{
    float lim = cruise_at(sim, r);
    for (uint32_t i = 0u; i < sim->n_stops; i++) {
        const RnSimStop *st = &sim->stops[i];
        if (st->along_m > sim->s && !st->used) {
            float v = sqrtf(st->speed_mps * st->speed_mps + 2.0f * RN_SIM_DECEL * (st->along_m - sim->s));
            lim = fminf(lim, v);
        }
    }
    return lim;
}

static void snap_to_stop(RnRideSim *sim)
{
    for (uint32_t i = 0u; i < sim->n_stops; i++) {
        RnSimStop *st = &sim->stops[i];
        float gap = st->along_m - sim->s;
        if (!st->used && st->speed_mps == 0.0f && gap >= 0.0f && gap < STOP_SNAP_M) {
            sim->s = st->along_m;
            sim->v = 0.0f;
            sim->dwell = st->dwell_s;
            st->used = true;
            return;
        }
    }
}

bool rn_ridesim_step(RnRideSim *sim, const RnRoute *r, float dt)
{
    if (!RN_ASSERT(sim != NULL && r != NULL) || !RN_ASSERT(dt > 0.0f && dt <= 1.0f)) {
        return false;
    }
    if (sim->done) {
        sim->t += dt;
        return false;
    }
    if (sim->dwell > 0.0f) {
        sim->dwell -= dt;
    } else {
        sim->v = fminf(sim->v + RN_SIM_ACCEL * dt, speed_limit(sim, r));
        float next = sim->s + sim->v * dt;
        for (uint32_t i = 0u; i < sim->n_stops; i++) {   /* never step over a red light, whatever dt */
            const RnSimStop *st = &sim->stops[i];
            if (!st->used && st->speed_mps == 0.0f && st->along_m >= sim->s && st->along_m < next) {
                next = st->along_m;
            }
        }
        sim->s = next;
        snap_to_stop(sim);
        if (sim->s >= r->total_m) {
            sim->s = r->total_m;
            sim->v = 0.0f;
            sim->done = true;
        }
    }
    sim->t += dt;
    return !sim->done;
}

void rn_ridesim_fix(RnRideSim *sim, const RnRoute *r, float sigma_m, uint32_t t_ms,
                    double unix_s, int32_t utc_offset_s, RnFix *out)
{
    if (!RN_ASSERT(sim != NULL && r != NULL && out != NULL) || !RN_ASSERT(sigma_m >= 0.0f)) {
        return;
    }
    double lat = 0.0;
    double lon = 0.0;
    rn_route_latlon_at(r, sim->s, &lat, &lon);
    float hdg = rn_route_heading_at(r, sim->s) * (float)RN_RAD2DEG;
    out->t_ms = t_ms;
    out->unix_s = unix_s;
    out->utc_offset_s = utc_offset_s;
    out->lat = lat + (double)rn_ridesim_gauss(&sim->rng, sigma_m) / 110540.0;
    out->lon = lon + (double)rn_ridesim_gauss(&sim->rng, sigma_m) / (111320.0 * cos(lat * RN_DEG2RAD));
    out->heading_deg = fmodf(hdg + rn_ridesim_gauss(&sim->rng, 4.0f) + 720.0f, 360.0f);
    out->speed_mps = (sim->v > 0.1f) ? fmaxf(0.0f, sim->v + rn_ridesim_gauss(&sim->rng, 0.2f)) : 0.0f;
    out->accuracy_m = sigma_m * 1.5f;
}
