/*
 * rn_ffi_pack.c - FFI: corridor, map generation and map pack building.
 */
#include <string.h>

#include "rn/rn_crc.h"
#include "rn_ffi_ctx.h"
#include "rn/rn_delta.h"

int32_t rn_ffi_corridor_plan(int32_t zoom, int32_t buffer_px)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(zoom > 0 && zoom <= 20) || !RN_ASSERT(buffer_px >= 0 && buffer_px <= 4096)) {
        return RN_ERR_PARAM;
    }
    rn_status st = rn_corridor_plan(&c->corridor, &c->route, (uint8_t)zoom, (uint32_t)buffer_px);
    c->corridor_ok = (st == RN_OK);
    c->writer_open = false;
    return (st == RN_OK) ? (int32_t)c->corridor.n_keep : st;
}

int32_t rn_ffi_corridor_info(int32_t *origin_x, int32_t *origin_y, int32_t *tiles_w,
                             int32_t *tiles_h, int32_t *n_keep)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(origin_x != NULL && origin_y != NULL && tiles_w != NULL) ||
        !RN_ASSERT(tiles_h != NULL && n_keep != NULL)) {
        return RN_ERR_PARAM;
    }
    if (!c->corridor_ok) {
        return RN_ERR_STATE;
    }
    *origin_x = c->corridor.origin_x;
    *origin_y = c->corridor.origin_y;
    *tiles_w = (int32_t)c->corridor.tiles_w;
    *tiles_h = (int32_t)c->corridor.tiles_h;
    *n_keep = (int32_t)c->corridor.n_keep;
    return RN_OK;
}

int32_t rn_ffi_corridor_keep(int32_t cx, int32_t cy)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!c->corridor_ok || cx < 0 || cy < 0) {
        return 0;
    }
    return rn_corridor_keep(&c->corridor, (uint32_t)cx, (uint32_t)cy) ? 1 : 0;
}

int32_t rn_ffi_corridor_point_px(int32_t i, float *x, float *y)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(x != NULL && y != NULL) || !c->corridor_ok || i < 0 || (uint32_t)i >= c->route.n_pts) {
        return RN_ERR_PARAM;
    }
    RnVec2 p = rn_corridor_point_px(&c->corridor, &c->route, (uint32_t)i);
    *x = p.x;
    *y = p.y;
    return RN_OK;
}

int32_t rn_ffi_mapgen_source(int32_t source)
{
    if (source != (int32_t)RN_MAPGEN_SRC_OSM && source != (int32_t)RN_MAPGEN_SRC_GOOGLE) {
        return RN_ERR_PARAM;
    }
    rn_ffi_ctx()->mapgen.source = (uint8_t)source;
    return RN_OK;
}

int32_t rn_ffi_mapgen_block(const uint8_t *rgb, int32_t w, int32_t h, uint8_t *out)
{
    if (!RN_ASSERT(rgb != NULL && out != NULL) || !RN_ASSERT(w > 0 && h > 0)) {
        return RN_ERR_PARAM;
    }
    return rn_mapgen_block(rgb, (uint32_t)w, (uint32_t)h, out, &rn_ffi_ctx()->mapgen);
}

int32_t rn_ffi_pack_begin(void)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(c != NULL) || !c->corridor_ok) {
        return RN_ERR_STATE;
    }
    c->pack_ok = false;
    c->wire_len = 0u;
    rn_status st = rn_packw_begin(&c->writer, c->pack, RN_FFI_PACK_CAP, &c->corridor);
    c->writer_open = (st == RN_OK);
    return st;
}

int32_t rn_ffi_pack_add_tile(int32_t cx, int32_t cy, const uint8_t *classes)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(classes != NULL) || !RN_ASSERT(cx >= 0 && cy >= 0)) {
        return RN_ERR_PARAM;
    }
    if (!c->writer_open) {
        return RN_ERR_STATE;
    }
    return rn_packw_add_tile(&c->writer, (uint32_t)cx, (uint32_t)cy, classes);
}

static int32_t adopt_pack(RnFfiCtx *c, uint32_t len)
{
    rn_status st = rn_pack_open(&c->pack_view, c->pack, len);
    if (st != RN_OK) {
        c->pack_ok = false;
        return st;
    }
    c->pack_len = len;
    c->pack_crc = rn_crc32(c->pack, len);
    c->wire_len = 0u;                             /* a new pack is sent raw until compressed */
    c->wire_base_crc = 0u;
    c->pack_ok = true;
    rn_tile_cache_init(&c->cache);
    return (int32_t)len;
}

int32_t rn_ffi_pack_finish(void)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(c != NULL) || !c->writer_open) {
        return RN_ERR_STATE;
    }
    uint32_t len = 0u;
    rn_status st = rn_packw_finish(&c->writer, &c->route, &len);
    c->writer_open = false;
    if (st != RN_OK) {
        return st;
    }
    return adopt_pack(c, len);
}

int32_t rn_ffi_pack_copy(uint8_t *out, int32_t cap)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(out != NULL) || !c->pack_ok) {
        return RN_ERR_STATE;
    }
    if (cap < 0 || (uint32_t)cap < c->pack_len) {
        return RN_ERR_FULL;
    }
    (void)memcpy(out, c->pack, c->pack_len);
    return (int32_t)c->pack_len;
}

int32_t rn_ffi_wire_set(const uint8_t *z, int32_t len)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(z != NULL) || !c->pack_ok) {
        return RN_ERR_STATE;
    }
    if (len < 0 || (uint32_t)len > RN_FFI_PACK_CAP) {
        return RN_ERR_PARAM;
    }
    (void)memcpy(c->wire, z, (uint32_t)len);
    c->wire_len = (uint32_t)len;
    c->wire_base_crc = 0u;
    return len;
}

int32_t rn_ffi_wire_set_delta(const uint8_t *z, int32_t len, uint32_t base_crc)
{
    int32_t r = rn_ffi_wire_set(z, len);
    if (r > 0) {
        rn_ffi_ctx()->wire_base_crc = base_crc;
    }
    return r;
}

int32_t rn_ffi_delta(const uint8_t *base, int32_t base_len, uint8_t *out, int32_t cap)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(base != NULL && out != NULL) || !c->pack_ok || base_len <= 0 || cap <= 0) {
        return RN_ERR_STATE;
    }
    return rn_delta_encode(base, (uint32_t)base_len, c->pack, c->pack_len, out, (uint32_t)cap);
}

int32_t rn_ffi_wire_len(void)
{
    const RnFfiCtx *c = rn_ffi_ctx();
    if (!c->pack_ok) {
        return RN_ERR_STATE;
    }
    return (int32_t)((c->wire_len > 0u) ? c->wire_len : c->pack_len);
}

uint32_t rn_ffi_pack_crc32(void)
{
    return rn_ffi_ctx()->pack_ok ? rn_ffi_ctx()->pack_crc : 0u;
}

uint32_t rn_ffi_pack_route_id(void)
{
    return rn_ffi_ctx()->pack_ok ? rn_ffi_ctx()->pack_view.h.route_id : 0u;
}

int32_t rn_ffi_pack_load(const uint8_t *blob, int32_t len)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!RN_ASSERT(blob != NULL) || len <= 0 || (uint32_t)len > RN_FFI_PACK_CAP) {
        return RN_ERR_PARAM;
    }
    (void)memcpy(c->pack, blob, (uint32_t)len);
    return adopt_pack(c, (uint32_t)len);
}

int32_t rn_ffi_pack_class_at(int32_t x, int32_t y)
{
    RnFfiCtx *c = rn_ffi_ctx();
    if (!c->pack_ok) {
        return -1;
    }
    return rn_pack_class_at(&c->pack_view, &c->cache, x, y);
}
