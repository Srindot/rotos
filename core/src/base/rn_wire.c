/*
 * rn_wire.c - little-endian byte cursors.
 */
#include "rn/rn_wire.h"

#include <string.h>

RnReader rn_reader(const uint8_t *buf, uint32_t len)
{
    RnReader r;
    r.buf = buf;
    r.len = (buf != NULL) ? len : 0u;
    r.pos = 0u;
    r.ok = RN_ASSERT(buf != NULL || len == 0u);
    return r;
}

/* True when n more bytes can be consumed; latches failure otherwise. */
static bool rd_room(RnReader *r, uint32_t n)
{
    if (!RN_ASSERT(r != NULL)) {
        return false;
    }
    if (!r->ok || n > r->len || r->pos > r->len - n) {
        r->ok = false;
        return false;
    }
    return true;
}

uint8_t rn_rd_u8(RnReader *r)
{
    uint8_t v = 0u;
    if (rd_room(r, 1u)) {
        v = r->buf[r->pos];
        r->pos += 1u;
    }
    return v;
}

uint16_t rn_rd_u16(RnReader *r)
{
    uint16_t v = 0u;
    if (rd_room(r, 2u)) {
        v = rn_get_u16(r->buf, r->pos);
        r->pos += 2u;
    }
    return v;
}

uint32_t rn_rd_u32(RnReader *r)
{
    uint32_t v = 0u;
    if (rd_room(r, 4u)) {
        v = rn_get_u32(r->buf, r->pos);
        r->pos += 4u;
    }
    return v;
}

int16_t rn_rd_i16(RnReader *r)
{
    return (int16_t)rn_rd_u16(r);
}

int32_t rn_rd_i32(RnReader *r)
{
    return (int32_t)rn_rd_u32(r);
}

float rn_rd_f32(RnReader *r)
{
    uint32_t bits = rn_rd_u32(r);
    float v;
    (void)memcpy(&v, &bits, sizeof v);
    return v;
}

void rn_rd_bytes(RnReader *r, uint8_t *out, uint32_t n)
{
    if (!RN_ASSERT(out != NULL || n == 0u)) {
        return;
    }
    if (rd_room(r, n) && n > 0u) {
        (void)memcpy(out, &r->buf[r->pos], n);
        r->pos += n;
    }
}

void rn_rd_skip(RnReader *r, uint32_t n)
{
    if (rd_room(r, n)) {
        r->pos += n;
    }
}

RnWriter rn_writer(uint8_t *buf, uint32_t cap)
{
    RnWriter w;
    w.buf = buf;
    w.cap = (buf != NULL) ? cap : 0u;
    w.pos = 0u;
    w.ok = RN_ASSERT(buf != NULL || cap == 0u);
    return w;
}

static bool wr_room(RnWriter *w, uint32_t n)
{
    if (!RN_ASSERT(w != NULL)) {
        return false;
    }
    if (!w->ok || n > w->cap || w->pos > w->cap - n) {
        w->ok = false;
        return false;
    }
    return true;
}

void rn_wr_u8(RnWriter *w, uint8_t v)
{
    if (wr_room(w, 1u)) {
        w->buf[w->pos] = v;
        w->pos += 1u;
    }
}

void rn_wr_u16(RnWriter *w, uint16_t v)
{
    if (wr_room(w, 2u)) {
        rn_put_u16(w->buf, w->pos, v);
        w->pos += 2u;
    }
}

void rn_wr_u32(RnWriter *w, uint32_t v)
{
    if (wr_room(w, 4u)) {
        rn_put_u32(w->buf, w->pos, v);
        w->pos += 4u;
    }
}

void rn_wr_i16(RnWriter *w, int16_t v)
{
    rn_wr_u16(w, (uint16_t)v);
}

void rn_wr_i32(RnWriter *w, int32_t v)
{
    rn_wr_u32(w, (uint32_t)v);
}

void rn_wr_f32(RnWriter *w, float v)
{
    uint32_t bits;
    (void)memcpy(&bits, &v, sizeof bits);
    rn_wr_u32(w, bits);
}

void rn_wr_bytes(RnWriter *w, const uint8_t *src, uint32_t n)
{
    if (!RN_ASSERT(src != NULL || n == 0u)) {
        return;
    }
    if (wr_room(w, n) && n > 0u) {
        (void)memcpy(&w->buf[w->pos], src, n);
        w->pos += n;
    }
}

void rn_wr_zeros(RnWriter *w, uint32_t n)
{
    if (wr_room(w, n) && n > 0u) {
        (void)memset(&w->buf[w->pos], 0, n);
        w->pos += n;
    }
}

uint16_t rn_get_u16(const uint8_t *p, uint32_t off)
{
    return (uint16_t)((uint32_t)p[off] | ((uint32_t)p[off + 1u] << 8));
}

uint32_t rn_get_u32(const uint8_t *p, uint32_t off)
{
    return (uint32_t)p[off] | ((uint32_t)p[off + 1u] << 8) |
           ((uint32_t)p[off + 2u] << 16) | ((uint32_t)p[off + 3u] << 24);
}

int32_t rn_get_i32(const uint8_t *p, uint32_t off)
{
    return (int32_t)rn_get_u32(p, off);
}

float rn_get_f32(const uint8_t *p, uint32_t off)
{
    uint32_t bits = rn_get_u32(p, off);
    float v;
    (void)memcpy(&v, &bits, sizeof v);
    return v;
}

void rn_put_u16(uint8_t *p, uint32_t off, uint16_t v)
{
    p[off] = (uint8_t)(v & 0xFFu);
    p[off + 1u] = (uint8_t)(v >> 8);
}

void rn_put_u32(uint8_t *p, uint32_t off, uint32_t v)
{
    p[off] = (uint8_t)(v & 0xFFu);
    p[off + 1u] = (uint8_t)((v >> 8) & 0xFFu);
    p[off + 2u] = (uint8_t)((v >> 16) & 0xFFu);
    p[off + 3u] = (uint8_t)(v >> 24);
}
