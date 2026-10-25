/*
 * rn_packwriter.c - map pack builder.
 */
#include "rn/rn_packwriter.h"

#include <string.h>

#include "rn/rn_wire.h"

#define UNSET_OFFSET 0xFFFFFFFFu

static uint32_t align4(uint32_t v)
{
    return (v + 3u) & ~3u;
}

static uint32_t slot_offset_pos(const RnPackWriter *w, uint32_t slot)
{
    return w->h.off_slots + slot * 4u;
}

/* Index and slot table: slots are the kept cells in row-major order. */
static void write_index(RnPackWriter *w)
{
    const RnCorridor *c = w->corridor;
    uint32_t slot = 0u;
    for (uint32_t cy = 0u; cy < c->tiles_h; cy++) {
        for (uint32_t cx = 0u; cx < c->tiles_w; cx++) {
            uint32_t cell = cy * c->tiles_w + cx;
            bool keep = rn_corridor_keep(c, cx, cy);
            rn_put_u16(w->out, w->h.off_index + cell * 2u, keep ? (uint16_t)slot : (uint16_t)RN_PACK_NO_TILE);
            if (keep) {
                rn_put_u32(w->out, slot_offset_pos(w, slot), UNSET_OFFSET);
                slot++;
            }
        }
    }
}

rn_status rn_packw_begin(RnPackWriter *w, uint8_t *out, uint32_t cap, const RnCorridor *c)
{
    if (!RN_ASSERT(w != NULL && out != NULL && c != NULL) || !RN_ASSERT(c->n_keep <= RN_PACK_MAX_SLOTS)) {
        return RN_ERR_PARAM;
    }
    (void)memset(w, 0, sizeof *w);
    w->out = out;
    w->cap = cap;
    w->corridor = c;
    w->n_slots = c->n_keep;
    uint32_t cells = (uint32_t)c->tiles_w * (uint32_t)c->tiles_h;
    w->h.off_index = RN_PACK_HEADER;
    w->h.off_slots = align4(w->h.off_index + cells * 2u);
    w->h.off_tiles = w->h.off_slots + w->n_slots * 4u;
    if (w->h.off_tiles > cap) {
        w->failed = true;
        return RN_ERR_FULL;
    }
    (void)memset(out, 0, w->h.off_tiles);
    write_index(w);
    w->data_pos = w->h.off_tiles;
    return RN_OK;
}

static rn_status put_tile(RnPackWriter *w, uint32_t slot, const uint8_t *classes)
{
    uint32_t pos = slot_offset_pos(w, slot);
    if (rn_get_u32(w->out, pos) != UNSET_OFFSET) {
        return RN_ERR_STATE;                      /* added twice */
    }
    int32_t n = rn_rle_encode(classes, &w->out[w->data_pos], w->cap - w->data_pos);
    if (n < 0) {
        w->failed = true;
        return (rn_status)n;
    }
    rn_put_u32(w->out, pos, w->data_pos - w->h.off_tiles);
    w->data_pos += (uint32_t)n;
    w->n_added++;
    return RN_OK;
}

rn_status rn_packw_add_tile(RnPackWriter *w, uint32_t cx, uint32_t cy, const uint8_t *classes)
{
    if (!RN_ASSERT(w != NULL && classes != NULL) || !RN_ASSERT(w->out != NULL)) {
        return RN_ERR_PARAM;
    }
    if (w->failed) {
        return RN_ERR_STATE;
    }
    const RnCorridor *c = w->corridor;
    if (cx >= c->tiles_w || cy >= c->tiles_h) {
        return RN_ERR_RANGE;
    }
    uint16_t slot = rn_get_u16(w->out, w->h.off_index + (cy * c->tiles_w + cx) * 2u);
    if (slot == (uint16_t)RN_PACK_NO_TILE) {
        return RN_ERR_RANGE;                      /* not in the corridor */
    }
    return put_tile(w, slot, classes);
}

static rn_status fill_missing(RnPackWriter *w)
{
    static const uint8_t LAND_TILE[RN_TILE_PIXELS] = {0u};
    for (uint32_t s = 0u; s < w->n_slots && s < RN_PACK_MAX_SLOTS; s++) {
        if (rn_get_u32(w->out, slot_offset_pos(w, s)) == UNSET_OFFSET) {
            rn_status st = put_tile(w, s, LAND_TILE);
            if (st != RN_OK) {
                return st;
            }
        }
    }
    return RN_OK;
}

static void write_route(RnPackWriter *w, const RnRoute *r, RnWriter *o)
{
    for (uint32_t i = 0u; i < r->n_pts && i < RN_MAX_ROUTE_PTS; i++) {
        RnVec2 p = rn_corridor_point_px(w->corridor, r, i);
        rn_wr_f32(o, p.x);
        rn_wr_f32(o, p.y);
        rn_wr_f32(o, r->pts[i].along_m);
    }
    w->h.off_mans = w->h.off_points + o->pos;
    for (uint32_t i = 0u; i < r->n_man && i < RN_MAX_MANEUVERS; i++) {
        const RnManeuver *m = &r->man[i];
        uint8_t name[RN_ROAD_NAME_MAX];
        (void)memset(name, 0, sizeof name);
        (void)memcpy(name, m->road, RN_ROAD_NAME_MAX);
        rn_wr_u8(o, m->type);
        rn_wr_u8(o, m->hint);
        rn_wr_u16(o, m->point_idx);
        rn_wr_f32(o, m->along_m);
        rn_wr_bytes(o, name, RN_ROAD_NAME_MAX);
    }
}

static void write_header(const RnPackWriter *w, const RnRoute *r, RnWriter *o)
{
    const RnCorridor *c = w->corridor;
    rn_wr_bytes(o, (const uint8_t *)RN_PACK_MAGIC, 4u);
    rn_wr_u16(o, (uint16_t)RN_PACK_VERSION);
    rn_wr_u8(o, c->zoom);
    rn_wr_u8(o, (uint8_t)RN_TILE);
    rn_wr_i32(o, c->origin_x);
    rn_wr_i32(o, c->origin_y);
    rn_wr_u16(o, c->tiles_w);
    rn_wr_u16(o, c->tiles_h);
    rn_wr_u16(o, (uint16_t)w->n_slots);
    rn_wr_u16(o, (uint16_t)r->n_man);
    rn_wr_u32(o, r->n_pts);
    rn_wr_f32(o, c->m_per_px);
    rn_wr_f32(o, r->total_m);
    rn_wr_u32(o, (uint32_t)r->duration_s);
    rn_wr_u32(o, r->route_id);
    rn_wr_u32(o, w->h.off_index);
    rn_wr_u32(o, w->h.off_slots);
    rn_wr_u32(o, w->h.off_tiles);
    rn_wr_u32(o, w->h.off_points);
    rn_wr_u32(o, w->h.off_mans);
    rn_wr_u32(o, w->h.total_len);
    rn_wr_u32(o, 0u);
}

rn_status rn_packw_finish(RnPackWriter *w, const RnRoute *r, uint32_t *out_len)
{
    if (!RN_ASSERT(w != NULL && r != NULL && out_len != NULL) || !RN_ASSERT(r->n_pts >= 2u)) {
        return RN_ERR_PARAM;
    }
    if (w->failed) {
        return RN_ERR_STATE;
    }
    rn_status st = fill_missing(w);
    if (st != RN_OK) {
        return st;
    }
    w->h.off_points = align4(w->data_pos);
    if (w->h.off_points > w->cap) {
        return RN_ERR_FULL;
    }
    for (uint32_t pad = w->data_pos; pad < w->h.off_points; pad++) {
        w->out[pad] = 0u;                         /* deterministic bytes */
    }
    RnWriter o = rn_writer(&w->out[w->h.off_points], w->cap - w->h.off_points);
    write_route(w, r, &o);
    if (!o.ok) {
        return RN_ERR_FULL;
    }
    w->h.total_len = w->h.off_points + o.pos;
    RnWriter hdr = rn_writer(w->out, RN_PACK_HEADER);
    write_header(w, r, &hdr);
    if (!hdr.ok) {
        return RN_ERR_FULL;
    }
    *out_len = w->h.total_len;
    return RN_OK;
}
