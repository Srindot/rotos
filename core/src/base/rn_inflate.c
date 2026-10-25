/*
 * rn_inflate.c - streaming raw-deflate decoder, after Mark Adler's puff.c
 * (canonical Huffman decoding one bit at a time: small and simple), made
 * resumable across chunks by the input margin rule in rn_inflate.h.
 */
#include "rn/rn_inflate.h"

#include <string.h>

#define MODE_HEADER 0u   /* expecting a block header */
#define MODE_STORED 1u   /* inside a stored block    */
#define MODE_CODES  2u   /* inside a Huffman block   */
#define MODE_DONE   3u
#define MODE_ERROR  4u

#define MAXBITS   15u
#define NLITLEN   288u
#define NDIST     30u
#define ERR_SYM   0xFFFFu

static const uint16_t LEN_BASE[29] = {3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 13u, 15u, 17u, 19u, 23u, 27u,
                                      31u, 35u, 43u, 51u, 59u, 67u, 83u, 99u, 115u, 131u, 163u, 195u, 227u, 258u};
static const uint8_t LEN_EXTRA[29] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u,
                                      2u, 3u, 3u, 3u, 3u, 4u, 4u, 4u, 4u, 5u, 5u, 5u, 5u, 0u};
static const uint16_t DIST_BASE[30] = {1u, 2u, 3u, 4u, 5u, 7u, 9u, 13u, 17u, 25u, 33u, 49u, 65u, 97u, 129u,
                                       193u, 257u, 385u, 513u, 769u, 1025u, 1537u, 2049u, 3073u, 4097u,
                                       6145u, 8193u, 12289u, 16385u, 24577u};
static const uint8_t DIST_EXTRA[30] = {0u, 0u, 0u, 0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u, 6u,
                                       6u, 7u, 7u, 8u, 8u, 9u, 9u, 10u, 10u, 11u, 11u, 12u, 12u, 13u, 13u};
static const uint8_t CL_ORDER[19] = {16u, 17u, 18u, 0u, 8u, 7u, 9u, 6u, 10u, 5u, 11u, 4u, 12u, 3u, 13u,
                                     2u, 14u, 1u, 15u};

void rn_inflate_init(RnInflate *z)
{
    if (!RN_ASSERT(z != NULL)) {
        return;
    }
    (void)memset(z, 0, sizeof *z);
    z->mode = MODE_HEADER;
}

rn_status rn_inflate_feed(RnInflate *z, const uint8_t *in, uint32_t n)
{
    if (!RN_ASSERT(z != NULL && in != NULL) || !RN_ASSERT(z->in_pos <= z->in_len)) {
        return RN_ERR_PARAM;
    }
    uint32_t keep = z->in_len - z->in_pos;
    if (n > RN_INFLATE_IN_CAP - keep) {
        return RN_ERR_FULL;
    }
    (void)memmove(z->in, &z->in[z->in_pos], keep);
    (void)memcpy(&z->in[keep], in, n);
    z->in_len = keep + n;
    z->in_pos = 0u;
    return RN_OK;
}

bool rn_inflate_done(const RnInflate *z)
{
    return RN_ASSERT(z != NULL) && z->mode == MODE_DONE && z->copy_len == 0u;
}

/* Input bytes still to decode, counting whole bytes held in the bit buffer. */
static uint32_t available(const RnInflate *z)
{
    return (z->in_len - z->in_pos) + z->bitcnt / 8u;
}

/* Next n (<= 16) bits, LSB first; marks the stream corrupt when input runs out. */
static uint32_t bits(RnInflate *z, uint32_t n)
{
    for (uint32_t k = 0u; k < 4u && z->bitcnt < n; k++) {
        if (z->in_pos >= z->in_len) {
            z->mode = MODE_ERROR;
            return 0u;
        }
        z->bitbuf |= (uint32_t)z->in[z->in_pos++] << z->bitcnt;
        z->bitcnt += 8u;
    }
    uint32_t v = z->bitbuf & ((1u << n) - 1u);
    z->bitbuf >>= n;
    z->bitcnt -= n;
    return v;
}

/* Canonical Huffman table from code lengths; false if over-subscribed. */
static bool construct(RnHuffman *h, const uint8_t *length, uint32_t n)
{
    uint16_t offs[MAXBITS + 1u];
    (void)memset(h->count, 0, sizeof h->count);
    for (uint32_t s = 0u; s < n; s++) {
        h->count[length[s]]++;
    }
    int32_t left = 1;
    for (uint32_t len = 1u; len <= MAXBITS; len++) {
        left = left * 2 - (int32_t)h->count[len];
        if (left < 0) {
            return false;
        }
    }
    offs[1] = 0u;
    for (uint32_t len = 1u; len < MAXBITS; len++) {
        offs[len + 1u] = (uint16_t)(offs[len] + h->count[len]);
    }
    for (uint32_t s = 0u; s < n; s++) {
        if (length[s] != 0u) {
            h->symbol[offs[length[s]]++] = (uint16_t)s;
        }
    }
    return true;
}

/* One symbol, bit by bit along the canonical code; ERR_SYM if invalid. */
static uint32_t decode(RnInflate *z, const RnHuffman *h)
{
    int32_t code = 0;
    int32_t first = 0;
    int32_t index = 0;
    for (uint32_t len = 1u; len <= MAXBITS; len++) {
        code |= (int32_t)bits(z, 1u);
        int32_t count = (int32_t)h->count[len];
        if (code - count < first) {
            return h->symbol[index + (code - first)];
        }
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return ERR_SYM;
}

static void fixed_tables(RnInflate *z)
{
    uint8_t len[NLITLEN];
    for (uint32_t s = 0u; s < NLITLEN; s++) {
        len[s] = (s < 144u) ? 8u : (s < 256u) ? 9u : (s < 280u) ? 7u : 8u;
    }
    (void)construct(&z->lencode, len, NLITLEN);
    for (uint32_t s = 0u; s < NDIST; s++) {
        len[s] = 5u;
    }
    (void)construct(&z->distcode, len, NDIST);
}

/* Code lengths of the literal/length and distance codes, run-length coded. */
static bool read_lengths(RnInflate *z, const RnHuffman *cl, uint8_t *len, uint32_t total)
{
    uint32_t i = 0u;
    while (i < total && z->mode != MODE_ERROR) {
        uint32_t sym = decode(z, cl);
        uint32_t rep = 0u;
        uint8_t val = 0u;
        if (sym < 16u) {
            len[i++] = (uint8_t)sym;
            continue;
        }
        if (sym == 16u) {
            if (i == 0u) {
                return false;
            }
            val = len[i - 1u];
            rep = 3u + bits(z, 2u);
        } else if (sym == 17u) {
            rep = 3u + bits(z, 3u);
        } else if (sym == 18u) {
            rep = 11u + bits(z, 7u);
        } else {
            return false;
        }
        if (i + rep > total) {
            return false;
        }
        (void)memset(&len[i], val, rep);
        i += rep;
    }
    return z->mode != MODE_ERROR;
}

static bool dynamic_tables(RnInflate *z)
{
    uint8_t len[NLITLEN + NDIST];
    uint32_t nlen = bits(z, 5u) + 257u;
    uint32_t ndist = bits(z, 5u) + 1u;
    uint32_t ncode = bits(z, 4u) + 4u;
    if (nlen > 286u || ndist > NDIST) {
        return false;
    }
    (void)memset(len, 0, 19u);
    for (uint32_t i = 0u; i < ncode; i++) {
        len[CL_ORDER[i]] = (uint8_t)bits(z, 3u);
    }
    RnHuffman cl;
    if (!construct(&cl, len, 19u) || !read_lengths(z, &cl, len, nlen + ndist) || len[256] == 0u) {
        return false;
    }
    return construct(&z->lencode, len, nlen) && construct(&z->distcode, &len[nlen], ndist);
}

static void header(RnInflate *z)
{
    z->last = bits(z, 1u) != 0u;
    uint32_t type = bits(z, 2u);
    bool ok = z->mode != MODE_ERROR;
    if (ok && type == 0u) {
        z->bitbuf >>= z->bitcnt % 8u;                /* stored: skip to a byte boundary */
        z->bitcnt -= z->bitcnt % 8u;
        uint32_t n = bits(z, 16u);
        uint32_t nn = bits(z, 16u);
        ok = n == (~nn & 0xFFFFu);
        z->stored_left = n;
        z->mode = MODE_STORED;
    } else if (ok && type == 1u) {
        fixed_tables(z);
        z->mode = MODE_CODES;
    } else if (ok && type == 2u) {
        ok = dynamic_tables(z);
        z->mode = MODE_CODES;
    } else {
        ok = false;
    }
    if (!ok || z->mode == MODE_ERROR) {
        z->mode = MODE_ERROR;
    }
}

static void emit(RnInflate *z, uint8_t b, uint8_t *out, uint32_t *n)
{
    z->window[z->produced % RN_INFLATE_WINDOW] = b;
    z->produced++;
    out[(*n)++] = b;
}

/* Copy the pending match while out has room. */
static void copy_match(RnInflate *z, uint8_t *out, uint32_t cap, uint32_t *n)
{
    while (z->copy_len > 0u && *n < cap) {
        uint8_t b = z->window[(z->produced - z->copy_dist) % RN_INFLATE_WINDOW];
        emit(z, b, out, n);
        z->copy_len--;
    }
}

/* One literal or match (or end of block) of a Huffman block. */
static void symbol(RnInflate *z, uint8_t *out, uint32_t *n)
{
    uint32_t sym = decode(z, &z->lencode);
    if (sym < 256u) {
        emit(z, (uint8_t)sym, out, n);
        return;
    }
    if (sym == 256u) {
        z->mode = z->last ? MODE_DONE : MODE_HEADER;
        return;
    }
    sym -= 257u;
    if (sym >= 29u) {
        z->mode = MODE_ERROR;
        return;
    }
    uint32_t length = LEN_BASE[sym] + bits(z, LEN_EXTRA[sym]);
    uint32_t ds = decode(z, &z->distcode);
    if (ds >= NDIST) {
        z->mode = MODE_ERROR;
        return;
    }
    uint32_t dist = DIST_BASE[ds] + bits(z, DIST_EXTRA[ds]);
    if (dist > z->produced || dist > RN_INFLATE_WINDOW) {
        z->mode = MODE_ERROR;
        return;
    }
    z->copy_len = length;
    z->copy_dist = dist;
}

/* One decoding step; false when it must wait for input (or is finished). */
static bool step(RnInflate *z, uint8_t *out, uint32_t cap, uint32_t *n, bool final)
{
    if (z->copy_len > 0u) {
        copy_match(z, out, cap, n);
        return true;
    }
    if (z->mode == MODE_DONE || z->mode == MODE_ERROR || (!final && available(z) < RN_INFLATE_MARGIN)) {
        return false;
    }
    if (z->mode == MODE_HEADER) {
        header(z);
    } else if (z->mode == MODE_STORED) {
        if (z->stored_left == 0u) {
            z->mode = z->last ? MODE_DONE : MODE_HEADER;
        } else {
            emit(z, (uint8_t)bits(z, 8u), out, n);
            z->stored_left--;
        }
    } else {
        symbol(z, out, n);
    }
    return z->mode != MODE_ERROR;
}

int32_t rn_inflate_out(RnInflate *z, uint8_t *out, uint32_t cap, bool final)
{
    if (!RN_ASSERT(z != NULL && out != NULL && cap > 0u)) {
        return RN_ERR_PARAM;
    }
    uint32_t n = 0u;
    /* each step emits at most one byte or none (headers, end of block):
     * bounded by cap emitting steps plus the non-emitting ones between them */
    for (uint32_t guard = 0u; guard < 4u * cap + 64u && n < cap; guard++) {
        if (!step(z, out, cap, &n, final)) {
            break;
        }
    }
    if (z->mode == MODE_ERROR) {
        return RN_ERR_FORMAT;
    }
    return (int32_t)n;
}
