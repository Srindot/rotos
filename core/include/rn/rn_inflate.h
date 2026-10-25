/*
 * rn_inflate.h - streaming raw-deflate decoder (RFC 1951) for map transfers.
 *
 * The phone compresses a map pack with raw deflate and a 4 KB window
 * (zlib windowBits -12); ROTOS feeds the compressed MAP_CHUNKs in as they
 * arrive and writes the output straight to flash. Everything lives in one
 * caller-owned RnInflate (about 6.5 KB, no dynamic memory).
 *
 * Use:  rn_inflate_init(z);
 *       for each chunk: rn_inflate_feed(z, data, n);
 *                       while ((k = rn_inflate_out(z, buf, cap, false)) > 0) store(buf, k);
 *       at the end:     while ((k = rn_inflate_out(z, buf, cap, true)) > 0) store(buf, k);
 *                       rn_inflate_done(z) must then be true.
 *
 * Without `final`, rn_inflate_out only decodes while at least
 * RN_INFLATE_MARGIN input bytes are buffered, so no step can run out of
 * input half way (the largest step, a dynamic block header, needs < 290 B).
 */
#ifndef RN_INFLATE_H
#define RN_INFLATE_H

#include "rn/rn_types.h"

#define RN_INFLATE_WINDOW   4096u     /* must match the compressor's window */
#define RN_INFLATE_IN_CAP   1024u     /* buffered compressed input           */
#define RN_INFLATE_MARGIN   320u
#define RN_INFLATE_MAX_FEED (RN_INFLATE_IN_CAP - RN_INFLATE_MARGIN)

typedef struct {
    uint16_t count[16];               /* codes of each length 0..15 */
    uint16_t symbol[288];             /* symbols ordered by code    */
} RnHuffman;

typedef struct {
    uint8_t window[RN_INFLATE_WINDOW];
    uint8_t in[RN_INFLATE_IN_CAP];
    uint32_t in_len;
    uint32_t in_pos;
    uint32_t bitbuf;
    uint32_t bitcnt;
    uint32_t produced;                /* bytes output so far      */
    uint32_t stored_left;             /* stored block bytes left  */
    uint32_t copy_len;                /* pending match            */
    uint32_t copy_dist;
    uint8_t mode;
    bool last;                        /* current block is the final one */
    RnHuffman lencode;
    RnHuffman distcode;
} RnInflate;

void rn_inflate_init(RnInflate *z);
/* Buffer n compressed bytes. n <= RN_INFLATE_MAX_FEED and the previous
 * output must have been drained (rn_inflate_out returned 0). */
rn_status rn_inflate_feed(RnInflate *z, const uint8_t *in, uint32_t n);
/* Up to cap output bytes; 0 = needs more input (or done); < 0 = corrupt data. */
int32_t rn_inflate_out(RnInflate *z, uint8_t *out, uint32_t cap, bool final);
/* The final block was decoded completely. */
bool rn_inflate_done(const RnInflate *z);

#endif /* RN_INFLATE_H */
