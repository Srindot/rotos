/*
 * rn_ffi_sim.c - FFI: ride simulator.
 */
#include "rn_ffi_ctx.h"

int32_t rn_ffi_sim_start(uint32_t seed, float cruise_mps)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(cruise_mps > 0.0f && cruise_mps < 60.0f) || c->route.n_pts < 2u) {
        return RN_ERR_STATE;
    }
    rn_ridesim_init(&c->sim, &c->route, seed, cruise_mps);
    c->sim_ok = true;
    return RN_OK;
}

int32_t rn_ffi_sim_step(float dt)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(dt > 0.0f) || !c->sim_ok) {
        return RN_ERR_STATE;
    }
    if (dt > 1.0f) {
        return RN_ERR_PARAM;                          /* not "ride finished" (0) */
    }
    return rn_ridesim_step(&c->sim, &c->route, dt) ? 1 : 0;
}

int32_t rn_ffi_sim_fix(float sigma_m, double *lat, double *lon, float *speed_mps,
                       float *heading_deg, float *accuracy_m)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(lat != NULL && lon != NULL && speed_mps != NULL) ||
        !RN_ASSERT(heading_deg != NULL && accuracy_m != NULL) || !c->sim_ok) {
        return RN_ERR_STATE;
    }
    RnFix f;
    rn_ridesim_fix(&c->sim, &c->route, sigma_m, 0u, 0.0, 0, &f);
    *lat = f.lat;
    *lon = f.lon;
    *speed_mps = f.speed_mps;
    *heading_deg = f.heading_deg;
    *accuracy_m = f.accuracy_m;
    return RN_OK;
}

float rn_ffi_sim_along(void)
{
    return rn_ffi_ctx()->sim_ok ? rn_ffi_ctx()->sim.s : 0.0f;
}

float rn_ffi_sim_speed(void)
{
    return rn_ffi_ctx()->sim_ok ? rn_ffi_ctx()->sim.v : 0.0f;
}

float rn_ffi_sim_time(void)
{
    return rn_ffi_ctx()->sim_ok ? rn_ffi_ctx()->sim.t : 0.0f;
}
