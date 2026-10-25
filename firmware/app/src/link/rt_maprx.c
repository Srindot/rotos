/*
 * rt_maprx.c - map pack reception.
 */
#include "rt_maprx.h"

#include <string.h>

#include "hal_log.h"
#include "hal_storage.h"
#include "hal_time.h"
#include "rn/rn_crc.h"
#include "rn/rn_mappack.h"
#include "rn/rn_types.h"
#include "rt_config.h"

static RtMapRxReply reply(bool send, uint8_t status, uint32_t value)
{
    RtMapRxReply r = {send, status, value, false};
    return r;
}

void rt_maprx_init(RtMapRx *m)
{
    if (!RN_ASSERT(m != NULL)) {
        return;
    }
    (void)memset(m, 0, sizeof *m);
}

RtMapRxReply rt_maprx_begin(RtMapRx *m, const RnMapBegin *b, uint32_t now_ms, const uint8_t *base,
                            uint32_t base_len, uint32_t base_crc)
{
    if (!RN_ASSERT(m != NULL && b != NULL)) {
        return reply(true, RN_ACK_BAD_STATE, 0u);
    }
    m->active = false;
    if (b->total_len < RN_PACK_HEADER || b->total_len > hal_storage_map_capacity()) {
        m->failed++;
        return reply(true, RN_ACK_TOO_BIG, 0u);
    }
    bool patch = b->base_crc != 0u;
    if (patch && (base == NULL || base_crc != b->base_crc || b->wire_len == 0u)) {
        return reply(true, RN_ACK_NO_BASE, 0u);          /* the phone sends the whole pack */
    }
    if (!hal_storage_map_begin(b->total_len)) {
        m->failed++;
        return reply(true, RN_ACK_STORAGE, 0u);
    }
    uint32_t still = 0u;
    if (patch && hal_storage_map(&still) != base) {        /* a big pack took both slots: no base */
        hal_storage_map_abort();
        return reply(true, RN_ACK_NO_BASE, 0u);
    }
    m->patched = patch;
    m->base = base;
    m->base_len = base_len;
    rn_delta_init(&m->delta);
    hal_log("maprx", "flash erase ms", (int32_t)(hal_time_ms() - now_ms));
    m->active = true;
    m->compressed = b->wire_len > 0u;
    m->corrupt = false;
    m->pack_len = b->total_len;
    m->total = m->compressed ? b->wire_len : b->total_len;
    m->received = 0u;
    m->written = 0u;
    m->crc_expected = b->crc32;
    m->crc_running = RN_CRC32_INIT;
    m->route_id = b->route_id;
    m->chunks_since_ack = 0u;
    m->started_ms = now_ms;
    m->last_rx_ms = now_ms;
    if (m->compressed) {
        rn_inflate_init(&m->inf);
    }
    return reply(true, RN_ACK_OK, 0u);
}

/* Store n pack bytes at the write position (CRC as they go). */
static bool store(RtMapRx *m, const uint8_t *bytes, uint32_t n)
{
    if (m->written + n > m->pack_len || !hal_storage_map_write(m->written, bytes, n)) {
        return false;
    }
    m->crc_running = rn_crc32_update(bytes, n, m->crc_running);
    m->written += n;
    return true;
}

/* Patch bytes -> pack bytes (copies come from the base pack) -> flash. */
static bool unpatch(RtMapRx *m, const uint8_t *in, uint32_t n)
{
    static uint8_t out[512];                          /* keep it off the stack */
    uint32_t off = 0u;
    for (uint32_t k = 0u; k < RT_MAP_INFLATE_STEPS; k++) {
        uint32_t used = 0u;
        int32_t got = rn_delta_run(&m->delta, &in[off], n - off, &used, m->base, m->base_len, out,
                                   (uint32_t)sizeof out);
        if (got < 0 || (got > 0 && !store(m, out, (uint32_t)got))) {
            return false;
        }
        off += used;
        if (got == 0 && used == 0u) {
            return off == n;                          /* all consumed, nothing pending */
        }
    }
    return false;
}

/* Inflate whatever the buffered input allows; false on corrupt data or storage failure. */
static bool drain(RtMapRx *m, bool final)
{
    static uint8_t out[512];                          /* keep it off the stack */
    for (uint32_t k = 0u; k < RT_MAP_INFLATE_STEPS; k++) {
        int32_t n = rn_inflate_out(&m->inf, out, (uint32_t)sizeof out, final);
        if (n <= 0) {
            return n == 0 && (!final || !m->patched || unpatch(m, out, 0u));
        }
        bool ok = m->patched ? unpatch(m, out, (uint32_t)n) : store(m, out, (uint32_t)n);
        if (!ok) {
            return false;
        }
    }
    return false;                                     /* more output than any pack: corrupt */
}

RtMapRxReply rt_maprx_chunk(RtMapRx *m, const RnMapChunk *c)
{
    if (!RN_ASSERT(m != NULL && c != NULL) || !m->active) {
        return reply(false, RN_ACK_BAD_STATE, 0u);
    }
    if (c->offset != m->received) {                  /* out of order: resync */
        m->chunks_since_ack = 0u;
        return reply(true, RN_ACK_BAD_STATE, m->received);
    }
    if (c->n == 0u || m->received + c->n > m->total) {
        return reply(true, RN_ACK_TOO_BIG, m->received);
    }
    bool ok;
    if (m->compressed) {
        ok = rn_inflate_feed(&m->inf, c->data, c->n) == RN_OK && drain(m, false);
        m->corrupt = m->corrupt || !ok;               /* keep acking; MAP_END reports it */
        ok = true;
    } else {
        ok = store(m, c->data, c->n);
    }
    if (!ok) {
        m->active = false;
        m->failed++;
        hal_storage_map_abort();
        return reply(true, RN_ACK_STORAGE, m->received);
    }
    m->received += c->n;
    m->chunks_since_ack++;
    if (m->chunks_since_ack >= RT_MAP_ACK_EVERY || m->received == m->total) {
        m->chunks_since_ack = 0u;
        return reply(true, RN_ACK_OK, m->received);
    }
    return reply(false, RN_ACK_OK, m->received);
}

RtMapRxReply rt_maprx_end(RtMapRx *m, const RnMapEnd *e)
{
    if (!RN_ASSERT(m != NULL && e != NULL) || !m->active) {
        return reply(true, RN_ACK_BAD_STATE, 0u);
    }
    m->active = false;
    if (m->compressed && !m->corrupt && m->received == m->total) {
        m->corrupt = !drain(m, true) || !rn_inflate_done(&m->inf) || (m->patched && !rn_delta_done(&m->delta));
    }
    bool ok = !m->corrupt && e->total_len == m->pack_len && m->received == m->total &&
              m->written == m->pack_len && e->crc32 == m->crc_expected &&
              rn_crc32_final(m->crc_running) == m->crc_expected;
    if (!ok || !hal_storage_map_commit(m->pack_len)) {
        hal_storage_map_abort();
        m->failed++;
        return reply(true, RN_ACK_BAD_CRC, m->received);
    }
    m->completed++;
    hal_log("maprx", "map received in ms", (int32_t)(hal_time_ms() - m->started_ms));
    RtMapRxReply r = reply(true, RN_ACK_OK, m->pack_len);
    r.committed = true;
    return r;
}
