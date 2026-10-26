/*
 * rn_mappack.c - map pack reader and tile cache.
 */
#include "rn/rn_mappack.h"

#include <math.h>
#include <string.h>

#include "rn/rn_wire.h"

#define BSEARCH_STEPS 32u

static RnPackHeader parse_header(RnReader *r)
{
    RnPackHeader h;
    (void)memset(&h, 0, sizeof h);
    h.zoom = rn_rd_u8(r);
    rn_rd_skip(r, 1u);                      /* tile size, checked by caller */
    h.origin_x = rn_rd_i32(r);
    h.origin_y = rn_rd_i32(r);
    h.tiles_w = rn_rd_u16(r);
    h.tiles_h = rn_rd_u16(r);
    h.n_slots = rn_rd_u16(r);
    h.n_maneuvers = rn_rd_u16(r);
    h.n_points = rn_rd_u32(r);
    h.m_per_px = rn_rd_f32(r);
    h.total_m = rn_rd_f32(r);
    h.duration_s = rn_rd_u32(r);
    h.route_id = rn_rd_u32(r);
    h.off_index = rn_rd_u32(r);
    h.off_slots = rn_rd_u32(r);
    h.off_tiles = rn_rd_u32(r);
    h.off_points = rn_rd_u32(r);
    h.off_mans = rn_rd_u32(r);
    h.total_len = rn_rd_u32(r);
    return h;
}

/* Section [off, off + size) lies inside the blob. */
static bool section_ok(uint32_t len, uint32_t off, uint64_t size)
{
    return off >= RN_PACK_HEADER && (uint64_t)off + size <= (uint64_t)len;
}

static rn_status check_sections(const RnPackHeader *h, uint32_t len)
{
    uint64_t cells = (uint64_t)h->tiles_w * (uint64_t)h->tiles_h;
    bool ok = h->total_len == len && h->n_points >= 2u && h->n_points <= RN_MAX_ROUTE_PTS &&
              h->n_maneuvers <= RN_MAX_MANEUVERS && h->n_slots <= RN_PACK_MAX_SLOTS &&
              cells > 0u && h->m_per_px > 0.0f && isfinite(h->m_per_px) && isfinite(h->total_m) &&
              h->zoom >= 10u && h->zoom <= 20u &&
              section_ok(len, h->off_index, cells * 2u) &&
              section_ok(len, h->off_slots, (uint64_t)h->n_slots * 4u) &&
              section_ok(len, h->off_tiles, 0u) &&
              section_ok(len, h->off_points, (uint64_t)h->n_points * RN_PACK_POINT_RECORD) &&
              section_ok(len, h->off_mans, (uint64_t)h->n_maneuvers * RN_PACK_MAN_RECORD);
    return ok ? RN_OK : RN_ERR_FORMAT;
}

rn_status rn_pack_open(RnPack *p, const uint8_t *blob, uint32_t len)
{
    if (!RN_ASSERT(p != NULL) || !RN_ASSERT(blob != NULL || len == 0u)) {
        return RN_ERR_PARAM;
    }
    (void)memset(p, 0, sizeof *p);
    if (len < RN_PACK_HEADER || memcmp(blob, RN_PACK_MAGIC, 4u) != 0) {
        return RN_ERR_FORMAT;
    }
    RnReader r = rn_reader(blob, len);
    rn_rd_skip(&r, 4u);
    uint16_t version = rn_rd_u16(&r);
    if (version != RN_PACK_VERSION || blob[7] != (uint8_t)RN_TILE) {
        return RN_ERR_FORMAT;
    }
    RnPackHeader h = parse_header(&r);
    if (!r.ok || check_sections(&h, len) != RN_OK) {
        return RN_ERR_FORMAT;
    }
    p->blob = blob;
    p->len = len;
    p->h = h;
    float prev = -1.0f;                               /* NaN would reach the camera; the searches */
    bool ok = true;                                   /* need non-decreasing along                */
    for (uint32_t i = 0u; i < h.n_points && i < RN_MAX_ROUTE_PTS && ok; i++) {
        RnPackPoint pt = rn_pack_point(p, i);
        ok = isfinite(pt.x) && isfinite(pt.y) && isfinite(pt.along) && pt.along >= prev;
        prev = pt.along;
    }
    for (uint32_t i = 0u; i < h.n_maneuvers && i < RN_MAX_MANEUVERS && ok; i++) {
        ok = isfinite(rn_pack_maneuver_along(p, i));
    }
    if (!ok) {
        (void)memset(p, 0, sizeof *p);
        return RN_ERR_FORMAT;
    }
    return RN_OK;
}

RnPackPoint rn_pack_point(const RnPack *p, uint32_t i)
{
    RnPackPoint pt = {0.0f, 0.0f, 0.0f};
    if (!RN_ASSERT(p != NULL && p->blob != NULL) || !RN_ASSERT(i < p->h.n_points)) {
        return pt;
    }
    uint32_t off = p->h.off_points + i * RN_PACK_POINT_RECORD;
    pt.x = rn_get_f32(p->blob, off);
    pt.y = rn_get_f32(p->blob, off + 4u);
    pt.along = rn_get_f32(p->blob, off + 8u);
    return pt;
}

rn_status rn_pack_maneuver(const RnPack *p, uint32_t i, RnManeuver *out)
{
    if (!RN_ASSERT(p != NULL && p->blob != NULL && out != NULL) || !RN_ASSERT(i < p->h.n_maneuvers)) {
        return RN_ERR_PARAM;
    }
    uint32_t off = p->h.off_mans + i * RN_PACK_MAN_RECORD;
    out->type = p->blob[off];
    out->hint = p->blob[off + 1u];
    out->point_idx = rn_get_u16(p->blob, off + 2u);
    out->along_m = rn_get_f32(p->blob, off + 4u);
    (void)memcpy(out->road, &p->blob[off + 8u], RN_ROAD_NAME_MAX);
    out->road[RN_ROAD_NAME_MAX] = '\0';
    if (out->type >= (uint8_t)RN_M_COUNT) {
        out->type = (uint8_t)RN_M_STRAIGHT;
    }
    return RN_OK;
}

float rn_pack_maneuver_along(const RnPack *p, uint32_t i)
{
    if (!RN_ASSERT(p != NULL && p->blob != NULL) || !RN_ASSERT(i < p->h.n_maneuvers)) {
        return 0.0f;
    }
    return rn_get_f32(p->blob, p->h.off_mans + i * RN_PACK_MAN_RECORD + 4u);
}

uint8_t rn_pack_maneuver_type(const RnPack *p, uint32_t i)
{
    if (!RN_ASSERT(p != NULL && p->blob != NULL) || !RN_ASSERT(i < p->h.n_maneuvers)) {
        return (uint8_t)RN_M_NONE;
    }
    return p->blob[p->h.off_mans + i * RN_PACK_MAN_RECORD];
}

RnPackPoint rn_pack_position_at(const RnPack *p, float along)
{
    RnPackPoint first = rn_pack_point(p, 0u);
    if (p == NULL || p->blob == NULL || p->h.n_points < 2u || along <= first.along) {
        return first;
    }
    RnPackPoint last = rn_pack_point(p, p->h.n_points - 1u);
    if (along >= last.along) {
        return last;
    }
    uint32_t lo = 0u;
    uint32_t hi = p->h.n_points - 1u;
    for (uint32_t k = 0u; k < BSEARCH_STEPS && hi - lo > 1u; k++) {
        uint32_t mid = (lo + hi) / 2u;
        if (rn_pack_point(p, mid).along <= along) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    RnPackPoint a = rn_pack_point(p, lo);
    RnPackPoint b = rn_pack_point(p, hi);
    float t = (along - a.along) / fmaxf(b.along - a.along, 1e-3f);
    RnPackPoint out = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, along};
    return out;
}

uint16_t rn_pack_slot(const RnPack *p, int32_t cx, int32_t cy)
{
    if (!RN_ASSERT(p != NULL && p->blob != NULL)) {
        return (uint16_t)RN_PACK_NO_TILE;
    }
    if (cx < 0 || cy < 0 || cx >= (int32_t)p->h.tiles_w || cy >= (int32_t)p->h.tiles_h) {
        return (uint16_t)RN_PACK_NO_TILE;
    }
    uint32_t cell = (uint32_t)cy * p->h.tiles_w + (uint32_t)cx;
    uint16_t slot = rn_get_u16(p->blob, p->h.off_index + cell * 2u);
    return (slot < p->h.n_slots) ? slot : (uint16_t)RN_PACK_NO_TILE;
}

void rn_tile_cache_init(RnTileCache *c)
{
    if (!RN_ASSERT(c != NULL)) {
        return;
    }
    for (uint32_t s = 0u; s < RN_TILE_CACHE_SETS; s++) {
        for (uint32_t w = 0u; w < RN_TILE_CACHE_WAYS; w++) {
            c->tag[s][w] = -1;
        }
        c->victim[s] = 0u;
    }
    c->route_id = 0u;
    c->hits = 0u;
    c->misses = 0u;
    c->errors = 0u;
}

/* Spread neighbouring slots over different sets. */
static uint32_t set_of(uint16_t slot)
{
    uint32_t h = (uint32_t)slot * 2654435761u;
    return (h >> 16) % RN_TILE_CACHE_SETS;
}

static const uint8_t *cache_fill(RnTileCache *c, const RnPack *p, uint16_t slot, uint32_t set)
{
    uint32_t way = c->victim[set] % RN_TILE_CACHE_WAYS;
    c->victim[set] = (uint8_t)((way + 1u) % RN_TILE_CACHE_WAYS);
    uint32_t off = rn_get_u32(p->blob, p->h.off_slots + (uint32_t)slot * 4u);
    uint32_t start = p->h.off_tiles + off;
    if (off >= p->len || start >= p->len) {
        c->errors++;
        c->tag[set][way] = -1;
        return NULL;
    }
    int32_t used = rn_rle_decode_packed(&p->blob[start], p->len - start, c->px[set][way]);
    if (used < 0) {
        c->errors++;
        c->tag[set][way] = -1;
        return NULL;
    }
    c->tag[set][way] = (int32_t)slot;
    c->misses++;
    return c->px[set][way];
}

const uint8_t *rn_tile_cache_get(RnTileCache *c, const RnPack *p, uint16_t slot)
{
    if (!RN_ASSERT(c != NULL && p != NULL && p->blob != NULL) || slot >= p->h.n_slots) {
        return NULL;
    }
    if (c->route_id != p->h.route_id) {
        rn_tile_cache_init(c);              /* new pack: forget old tiles */
        c->route_id = p->h.route_id;
    }
    uint32_t set = set_of(slot);
    for (uint32_t w = 0u; w < RN_TILE_CACHE_WAYS; w++) {
        if (c->tag[set][w] == (int32_t)slot) {
            c->hits++;
            return c->px[set][w];
        }
    }
    return cache_fill(c, p, slot, set);
}

int32_t rn_pack_class_at(const RnPack *p, RnTileCache *c, int32_t x, int32_t y)
{
    if (x < 0 || y < 0) {
        return -1;
    }
    uint16_t slot = rn_pack_slot(p, x >> 5, y >> 5);
    if (slot == (uint16_t)RN_PACK_NO_TILE) {
        return -1;
    }
    const uint8_t *px = rn_tile_cache_get(c, p, slot);
    if (px == NULL) {
        return -1;
    }
    uint32_t i = (((uint32_t)y & 31u) << 5) | ((uint32_t)x & 31u);
    uint8_t b = px[i >> 1];
    return ((i & 1u) != 0u) ? (int32_t)(b & 0x0Fu) : (int32_t)(b >> 4);
}
