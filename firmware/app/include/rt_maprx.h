/*
 * rt_maprx.h - receives a map pack over the link into storage
 * (MAP_BEGIN / MAP_CHUNK... / MAP_END, see docs/PROTOCOL.md). A compressed
 * transfer (MAP_BEGIN wire_len > 0) is inflated chunk by chunk on its way
 * to flash.
 */
#ifndef RT_MAPRX_H
#define RT_MAPRX_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_delta.h"
#include "rn/rn_inflate.h"
#include "rn/rn_protocol.h"

typedef struct {
    bool active;
    bool compressed;
    bool corrupt;              /* inflate failed: the transfer ends in BAD_CRC */
    uint32_t total;            /* bytes on the wire                */
    uint32_t received;         /* wire bytes received so far       */
    uint32_t pack_len;         /* bytes to store                   */
    uint32_t written;          /* bytes stored so far              */
    uint32_t crc_expected;
    uint32_t crc_running;
    uint32_t route_id;
    uint32_t chunks_since_ack;
    uint32_t started_ms;
    uint32_t last_rx_ms;       /* last MAP_BEGIN / MAP_CHUNK (stall timeout) */
    uint32_t completed;        /* statistics */
    uint32_t failed;
    RnInflate inf;             /* ~6.5 KB, used by compressed transfers */
    bool patched;              /* the stream is a patch against base   */
    const uint8_t *base;       /* the current pack (other flash slot)  */
    uint32_t base_len;
    RnDelta delta;
} RtMapRx;

/* What the caller should answer, if anything. */
typedef struct {
    bool send_ack;
    uint8_t status;            /* RN_ACK_* */
    uint32_t value;
    bool committed;            /* a new pack is now stored */
} RtMapRxReply;

void rt_maprx_init(RtMapRx *m);
/* base / base_len / base_crc: the pack ROTOS holds now (NULL / 0 when none),
 * the base of a patched transfer. */
RtMapRxReply rt_maprx_begin(RtMapRx *m, const RnMapBegin *b, uint32_t now_ms, const uint8_t *base,
                            uint32_t base_len, uint32_t base_crc);
RtMapRxReply rt_maprx_chunk(RtMapRx *m, const RnMapChunk *c);
RtMapRxReply rt_maprx_end(RtMapRx *m, const RnMapEnd *e);

#endif /* RT_MAPRX_H */
