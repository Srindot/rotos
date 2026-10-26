/*
 * rn_ffi_route.c - FFI: session, route building and flyover hints.
 */
#include <string.h>

#include "rn/rn_polyline.h"
#include "rn/rn_sun.h"
#include "rn/rn_text.h"
#include "rn_ffi_ctx.h"

static RnFfiCtx s_ctx;

RnFfiCtx *rn_ffi_ctx(void)
{
    return &s_ctx;
}

int32_t rn_ffi_api_version(void)
{
    return RN_FFI_API_VERSION;
}

void rn_ffi_reset(void)
{
    RnFfiCtx *c = &s_ctx;
    rn_route_clear(&c->route);
    rn_bridges_clear(&c->bridges);
    c->corridor_ok = false;
    c->writer_open = false;
    c->pack_ok = false;
    c->pack_len = 0u;
    c->have_nav = false;
    c->sim_ok = false;
    rn_engine_init(&c->engine);
    rn_tile_cache_init(&c->cache);
    /* not the inbound link parser: a route load mid-ride must not drop a ROTOS frame that is
     * half received (the link resets it itself on (re)connect) */
}

uint32_t rn_ffi_assert_count(void)
{
    return rn_assert_count();
}

int32_t rn_ffi_route_add_polyline(const char *encoded)
{
    RnFfiCtx *c = &s_ctx;
    if (!RN_ASSERT(encoded != NULL)) {
        return RN_ERR_PARAM;
    }
    int32_t n = rn_polyline_decode(encoded, c->scratch_lat, c->scratch_lon, RN_MAX_ROUTE_PTS);
    if (n <= 0) {
        return (n == 0) ? RN_ERR_FORMAT : n;
    }
    /* The step starts at its first point - or, when rn_route_add_point dropped
     * it (within RN_ROUTE_MIN_STEP_M of the last stored point), at that point. */
    int32_t first = -1;
    for (int32_t i = 0; i < n; i++) {
        uint32_t before = c->route.n_pts;
        rn_status st = rn_route_add_point(&c->route, c->scratch_lat[i], c->scratch_lon[i]);
        if (st != RN_OK) {
            return st;
        }
        if (i == 0) {
            first = (c->route.n_pts > before) ? (int32_t)before : (int32_t)before - 1;
        }
    }
    return first;
}

int32_t rn_ffi_route_add_point(double lat, double lon)
{
    rn_status st = rn_route_add_point(&s_ctx.route, lat, lon);
    return (st == RN_OK) ? (int32_t)s_ctx.route.n_pts - 1 : st;
}

int32_t rn_ffi_route_point_count(void)
{
    return (int32_t)s_ctx.route.n_pts;
}

int32_t rn_ffi_route_add_maneuver(int32_t type, int32_t point_idx, const char *road)
{
    if (!RN_ASSERT(type >= 0 && type < (int32_t)RN_M_COUNT) || !RN_ASSERT(point_idx >= 0)) {
        return RN_ERR_PARAM;
    }
    return rn_route_add_maneuver(&s_ctx.route, (uint8_t)type, (uint32_t)point_idx, road);
}

int32_t rn_ffi_route_add_maneuver_google(const char *maneuver, int32_t point_idx,
                                         const char *instruction)
{
    if (!RN_ASSERT(maneuver != NULL) || !RN_ASSERT(point_idx >= 0)) {
        return RN_ERR_PARAM;
    }
    char road[RN_ROAD_NAME_MAX + 1u];
    (void)rn_text_road_from_instruction(road, (uint32_t)sizeof road, instruction);
    uint8_t type = rn_maneuver_from_google(maneuver);
    return rn_route_add_maneuver(&s_ctx.route, type, (uint32_t)point_idx, road);
}

int32_t rn_ffi_route_finish(float duration_s, uint32_t route_id)
{
    return rn_route_finish(&s_ctx.route, duration_s, route_id);
}

float rn_ffi_route_total_m(void)
{
    return s_ctx.route.total_m;
}

float rn_ffi_route_duration_s(void)
{
    return s_ctx.route.duration_s;
}

int32_t rn_ffi_route_maneuver_count(void)
{
    return (int32_t)s_ctx.route.n_man;
}

int32_t rn_ffi_route_maneuver(int32_t i, uint8_t *type, uint8_t *hint, float *along_m, char *road)
{
    const RnRoute *r = &s_ctx.route;
    uint32_t k = (i >= 0) ? (uint32_t)i : RN_MAX_MANEUVERS;
    if (!RN_ASSERT(type != NULL && hint != NULL && along_m != NULL && road != NULL) ||
        k >= r->n_man || k >= RN_MAX_MANEUVERS) {
        return RN_ERR_PARAM;
    }
    const RnManeuver *m = &r->man[k];
    *type = m->type;
    *hint = m->hint;
    *along_m = m->along_m;
    (void)rn_text_copy(road, RN_ROAD_NAME_MAX + 1u, m->road);
    return RN_OK;
}

int32_t rn_ffi_route_point(int32_t i, double *lat, double *lon, float *along_m)
{
    const RnRoute *r = &s_ctx.route;
    uint32_t k = (i >= 0) ? (uint32_t)i : RN_MAX_ROUTE_PTS;
    if (!RN_ASSERT(lat != NULL && lon != NULL && along_m != NULL) || k >= r->n_pts || k >= RN_MAX_ROUTE_PTS) {
        return RN_ERR_PARAM;
    }
    *lat = r->pts[k].lat;
    *lon = r->pts[k].lon;
    *along_m = r->pts[k].along_m;
    return RN_OK;
}

void rn_ffi_bridges_clear(void)
{
    rn_bridges_clear(&s_ctx.bridges);
}

int32_t rn_ffi_bridges_add(const double *lat, const double *lon, int32_t n)
{
    if (!RN_ASSERT(lat != NULL && lon != NULL) || n < 0) {
        return RN_ERR_PARAM;
    }
    return rn_bridges_add(&s_ctx.bridges, &s_ctx.route, lat, lon, (uint32_t)n);
}

int32_t rn_ffi_bridges_count(void)
{
    return (int32_t)s_ctx.bridges.n_bridges;
}

int32_t rn_ffi_flyover_annotate(void)
{
    return rn_flyover_annotate(&s_ctx.route, &s_ctx.bridges, &s_ctx.fly);
}

int32_t rn_ffi_flyover_by_name(void)
{
    return rn_flyover_by_name(&s_ctx.route);
}

double rn_ffi_sun_elevation(double lat, double lon, double unix_s)
{
    return rn_sun_elevation_deg(lat, lon, unix_s);
}

double rn_ffi_sun_next_sunset(double lat, double lon, double from_unix_s)
{
    return rn_sun_next_sunset(lat, lon, from_unix_s);
}

uint32_t rn_ffi_local_seconds(double unix_s, int32_t utc_offset_s)
{
    return rn_engine_local_seconds(unix_s, utc_offset_s);
}
