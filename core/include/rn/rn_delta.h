/*
 * rn_delta.h - map packs as a patch against the pack ROTOS already holds.
 *
 * Consecutive maps overlap (a reroute, a refreshed area, tomorrow's ride from
 * home): most tiles are byte-identical. The phone describes the new pack as
 *
 *   0x00 LIT   u16 n, then n bytes          new bytes
 *   0x01 COPY  u32 base_offset, u16 n       n bytes from the old pack
 *
 * and deflates that stream; ROTOS inflates it, decodes it here and writes
 * the result to flash while the old pack is still mapped in the other slot.
 */
#ifndef RN_DELTA_H
#define RN_DELTA_H

#include "rn/rn_types.h"

#define RN_DELTA_LIT        0u
#define RN_DELTA_COPY       1u
#define RN_DELTA_MIN_MATCH  32u        /* shorter repeats stay literal */
#define RN_DELTA_MAX_RUN    65535u

/* Streaming decoder: patch bytes in, pack bytes out. */
typedef struct {
    uint8_t hdr[7];
    uint32_t hdr_n;                   /* header bytes buffered       */
    uint8_t op;
    uint32_t left;                    /* bytes left in the current op */
    uint32_t base_off;                /* COPY: next byte of the base  */
    bool in_op;
    bool corrupt;
} RnDelta;

void rn_delta_init(RnDelta *d);
/* Consume patch bytes from in[0..n) (*used receives how many) and produce
 * up to cap output bytes. Returns bytes produced, < 0 on a corrupt patch.
 * Call again with the rest of the input while it produces or consumes. */
int32_t rn_delta_run(RnDelta *d, const uint8_t *in, uint32_t n, uint32_t *used, const uint8_t *base,
                     uint32_t base_len, uint8_t *out, uint32_t cap);
/* The patch ended exactly at an op boundary. */
bool rn_delta_done(const RnDelta *d);

/* Phone side: patch turning base into next; returns its length or < 0. */
int32_t rn_delta_encode(const uint8_t *base, uint32_t base_len, const uint8_t *next, uint32_t next_len,
                        uint8_t *out, uint32_t cap);

#endif /* RN_DELTA_H */
