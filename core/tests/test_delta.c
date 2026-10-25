/*
 * test_delta.c - map-pack patches: encode against a base, decode streaming
 * in small pieces (like inflate output on the device), corrupt patches fail.
 */
#include <string.h>

#include "rn/rn_delta.h"
#include "rn_test.h"

#define LEN (120u * 1024u)

static uint8_t g_base[LEN];
static uint8_t g_next[LEN + 8192u];
static uint8_t g_patch[LEN * 2u];
static uint8_t g_out[LEN + 8192u];

static void fill(uint8_t *b, uint32_t n, uint32_t seed)
{
    uint32_t x = seed;
    for (uint32_t i = 0u; i < n; i++) {
        x = x * 1664525u + 1013904223u;
        b[i] = (uint8_t)(x >> 24);
    }
}

/* Decode patch[0..plen) in pieces of `piece` bytes; returns output length or < 0. */
static int32_t decode(const uint8_t *patch, uint32_t plen, uint32_t piece)
{
    RnDelta d;
    rn_delta_init(&d);
    uint32_t got = 0u;
    uint8_t buf[300];
    for (uint32_t off = 0u; off < plen; ) {
        uint32_t n = (plen - off < piece) ? plen - off : piece;
        uint32_t used = 0u;
        int32_t k = rn_delta_run(&d, &patch[off], n, &used, g_base, LEN, buf, (uint32_t)sizeof buf);
        if (k < 0) {
            return k;
        }
        (void)memcpy(&g_out[got], buf, (uint32_t)k);
        got += (uint32_t)k;
        off += used;
        if (k == 0 && used == 0u) {
            return RN_ERR_FORMAT;                     /* stuck */
        }
    }
    for (uint32_t k = 0u; k < 100000u; k++) {         /* drain a final COPY */
        uint32_t used = 0u;
        int32_t m = rn_delta_run(&d, NULL, 0u, &used, g_base, LEN, buf, (uint32_t)sizeof buf);
        if (m <= 0) {
            break;
        }
        (void)memcpy(&g_out[got], buf, (uint32_t)m);
        got += (uint32_t)m;
    }
    return rn_delta_done(&d) ? (int32_t)got : RN_ERR_FORMAT;
}

static void test_overlap(void)
{
    fill(g_base, LEN, 1u);
    /* next: base with a changed middle, a moved block and 4 KB of new bytes at the end */
    (void)memcpy(g_next, g_base, LEN);
    fill(&g_next[40000], 3000u, 9u);
    (void)memcpy(&g_next[80000], &g_base[1000], 9000u);
    fill(&g_next[LEN], 4096u, 5u);
    uint32_t nlen = LEN + 4096u;
    int32_t plen = rn_delta_encode(g_base, LEN, g_next, nlen, g_patch, (uint32_t)sizeof g_patch);
    CHECK(plen > 0 && plen < 12000);                  /* ~7 KB new, the rest copied */
    for (uint32_t piece = 1u; piece <= 701u; piece += 350u) {
        int32_t got = decode(g_patch, (uint32_t)plen, piece);
        CHECK(got == (int32_t)nlen && memcmp(g_out, g_next, nlen) == 0);
    }
}

static void test_unrelated_and_corrupt(void)
{
    fill(g_base, LEN, 2u);
    fill(g_next, LEN, 3u);                            /* nothing in common: all literal */
    int32_t plen = rn_delta_encode(g_base, LEN, g_next, LEN, g_patch, (uint32_t)sizeof g_patch);
    CHECK(plen > (int32_t)LEN && plen < (int32_t)LEN + 64);
    CHECK(decode(g_patch, (uint32_t)plen, 240u) == (int32_t)LEN && memcmp(g_out, g_next, LEN) == 0);
    uint8_t bad[7] = {RN_DELTA_COPY, 0xF0, 0xFF, 0xFF, 0x00, 0x10, 0x00};   /* beyond the base */
    CHECK(decode(bad, 7u, 7u) < 0);
    uint8_t op[3] = {9u, 1u, 0u};                                           /* unknown op */
    CHECK(decode(op, 3u, 3u) < 0);
    CHECK(rn_delta_encode(g_base, LEN, g_next, LEN, g_patch, 100u) == RN_ERR_FULL);
}

void test_delta(void)
{
    (void)printf("delta\n");
    test_overlap();
    test_unrelated_and_corrupt();
}
