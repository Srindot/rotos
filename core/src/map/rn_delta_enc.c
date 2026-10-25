/*
 * rn_delta_enc.c - patch encoder (phone side only: a 4 MB hash table).
 *
 * Greedy: index every position of the base by a hash of its next 16 bytes;
 * walk the new pack, and wherever a candidate matches for at least
 * RN_DELTA_MIN_MATCH bytes emit a COPY (extended as far as it goes),
 * otherwise collect literal bytes.
 */
#include <string.h>

#include "rn/rn_delta.h"

#define HASH_BITS 20u
#define HASH_SIZE (1u << HASH_BITS)
#define KEY_LEN   16u
#define NO_POS    0xFFFFFFFFu

static uint32_t g_table[HASH_SIZE];

static uint32_t hash16(const uint8_t *p)
{
    uint32_t h = 2166136261u;                       /* FNV-1a */
    for (uint32_t i = 0u; i < KEY_LEN; i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h >> (32u - HASH_BITS);
}

typedef struct {
    uint8_t *out;
    uint32_t cap;
    uint32_t len;
    bool ok;
} Sink;

static void put(Sink *s, const uint8_t *b, uint32_t n)
{
    if (!s->ok || n > s->cap - s->len) {
        s->ok = false;
        return;
    }
    (void)memcpy(&s->out[s->len], b, n);
    s->len += n;
}

static void literal(Sink *s, const uint8_t *b, uint32_t n)
{
    for (uint32_t done = 0u; done < n;) {
        uint32_t k = (n - done < RN_DELTA_MAX_RUN) ? n - done : RN_DELTA_MAX_RUN;
        uint8_t h[3] = {RN_DELTA_LIT, (uint8_t)k, (uint8_t)(k >> 8)};
        put(s, h, 3u);
        put(s, &b[done], k);
        done += k;
    }
}

static void copy(Sink *s, uint32_t off, uint32_t n)
{
    uint8_t h[7] = {RN_DELTA_COPY, (uint8_t)off, (uint8_t)(off >> 8), (uint8_t)(off >> 16), (uint8_t)(off >> 24),
                    (uint8_t)n, (uint8_t)(n >> 8)};
    put(s, h, 7u);
}

static void index_base(const uint8_t *base, uint32_t len)
{
    for (uint32_t i = 0u; i < HASH_SIZE; i++) {
        g_table[i] = NO_POS;
    }
    for (uint32_t i = 0u; i + KEY_LEN <= len; i++) {
        g_table[hash16(&base[i])] = i;
    }
}

/* Length of the common run of base[b..] and next[p..], capped. */
static uint32_t match_len(const uint8_t *base, uint32_t base_len, uint32_t b, const uint8_t *next, uint32_t next_len,
                          uint32_t p)
{
    uint32_t n = 0u;
    while (n < RN_DELTA_MAX_RUN && b + n < base_len && p + n < next_len && base[b + n] == next[p + n]) {
        n++;
    }
    return n;
}

int32_t rn_delta_encode(const uint8_t *base, uint32_t base_len, const uint8_t *next, uint32_t next_len,
                        uint8_t *out, uint32_t cap)
{
    if (!RN_ASSERT(base != NULL && next != NULL && out != NULL)) {
        return RN_ERR_PARAM;
    }
    Sink s = {out, cap, 0u, true};
    index_base(base, base_len);
    uint32_t lit = 0u;                              /* start of pending literal bytes */
    uint32_t p = 0u;
    while (p < next_len && s.ok) {
        uint32_t cand = (p + KEY_LEN <= next_len) ? g_table[hash16(&next[p])] : NO_POS;
        uint32_t n = (cand != NO_POS) ? match_len(base, base_len, cand, next, next_len, p) : 0u;
        if (n < RN_DELTA_MIN_MATCH) {
            p++;
            continue;
        }
        literal(&s, &next[lit], p - lit);
        copy(&s, cand, n);
        p += n;
        lit = p;
    }
    literal(&s, &next[lit], next_len - lit);
    return s.ok ? (int32_t)s.len : RN_ERR_FULL;
}
