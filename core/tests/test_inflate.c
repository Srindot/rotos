/*
 * test_inflate.c - the streaming decoder against zlib (raw deflate, 4 KB
 * window, the phone's settings): every block type, chunked like MAP_CHUNKs,
 * and corrupt input that must fail cleanly.
 */
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "rn/rn_inflate.h"
#include "rn_test.h"

#define SRC_LEN (200u * 1024u)

static uint8_t g_src[SRC_LEN];
static uint8_t g_z[SRC_LEN + 4096u];
static uint8_t g_out[SRC_LEN + 4096u];
static RnInflate g_inf;

/* Map-like bytes: long runs of a few values, some repeats from far back. */
static void make_source(uint32_t seed)
{
    uint32_t x = seed;
    uint32_t i = 0u;
    while (i < SRC_LEN) {
        x = x * 1103515245u + 12345u;
        uint32_t run = 1u + (x >> 24) % 40u;
        uint8_t v = (uint8_t)((x >> 8) % 9u);
        for (uint32_t k = 0u; k < run && i < SRC_LEN; k++) {
            g_src[i++] = v;
        }
    }
}

static uint32_t zcompress(const uint8_t *src, uint32_t n, int level, int strategy)
{
    z_stream s;
    (void)memset(&s, 0, sizeof s);
    CHECK(deflateInit2(&s, level, Z_DEFLATED, -12, 9, strategy) == Z_OK);
    s.next_in = (Bytef *)src;
    s.avail_in = n;
    s.next_out = g_z;
    s.avail_out = (uInt)sizeof g_z;
    CHECK(deflate(&s, Z_FINISH) == Z_STREAM_END);
    uint32_t out = (uint32_t)s.total_out;
    (void)deflateEnd(&s);
    return out;
}

/* Feed z in pieces of `chunk`, draining after each; returns bytes out or < 0. */
static int32_t inflate_chunked(const uint8_t *z, uint32_t zlen, uint32_t chunk, uint32_t outcap)
{
    rn_inflate_init(&g_inf);
    uint32_t got = 0u;
    uint8_t buf[512];
    for (uint32_t off = 0u; off <= zlen; off += chunk) {
        bool final = off + chunk > zlen;
        uint32_t n = final ? zlen - off : chunk;
        if (n > 0u && rn_inflate_feed(&g_inf, &z[off], n) != RN_OK) {
            return RN_ERR_FULL;
        }
        for (uint32_t k = 0u; k < 100000u; k++) {
            int32_t m = rn_inflate_out(&g_inf, buf, (uint32_t)sizeof buf, final);
            if (m <= 0) {
                if (m < 0) {
                    return m;
                }
                break;
            }
            if (got + (uint32_t)m > outcap) {
                return RN_ERR_FULL;
            }
            (void)memcpy(&g_out[got], buf, (uint32_t)m);
            got += (uint32_t)m;
        }
        if (final) {
            break;
        }
    }
    return rn_inflate_done(&g_inf) ? (int32_t)got : RN_ERR_FORMAT;
}

static void roundtrip(uint32_t n, int level, int strategy, uint32_t chunk)
{
    uint32_t zlen = zcompress(g_src, n, level, strategy);
    int32_t got = inflate_chunked(g_z, zlen, chunk, (uint32_t)sizeof g_out);
    CHECK(got == (int32_t)n);
    CHECK(got == (int32_t)n && memcmp(g_out, g_src, n) == 0);
}

static void test_block_types(void)
{
    make_source(7u);
    roundtrip(SRC_LEN, 9, Z_DEFAULT_STRATEGY, 240u);   /* dynamic blocks */
    roundtrip(SRC_LEN, 9, Z_FIXED, 240u);              /* fixed codes    */
    roundtrip(70000u, 0, Z_DEFAULT_STRATEGY, 240u);    /* stored blocks  */
    roundtrip(SRC_LEN, 6, Z_DEFAULT_STRATEGY, 1u);     /* byte by byte   */
    roundtrip(SRC_LEN, 9, Z_DEFAULT_STRATEGY, RN_INFLATE_MAX_FEED);
    roundtrip(1u, 9, Z_DEFAULT_STRATEGY, 240u);
    for (uint32_t i = 0u; i < SRC_LEN; i++) {          /* incompressible */
        g_src[i] = (uint8_t)((i * 2654435761u) >> 13);
    }
    roundtrip(SRC_LEN, 9, Z_DEFAULT_STRATEGY, 240u);
}

static void test_corrupt(void)
{
    make_source(11u);
    uint32_t zlen = zcompress(g_src, SRC_LEN, 9, Z_DEFAULT_STRATEGY);
    CHECK(inflate_chunked(g_z, zlen / 2u, 240u, (uint32_t)sizeof g_out) < 0);   /* truncated */
    uint32_t bad = 0u;
    for (uint32_t trial = 0u; trial < 40u; trial++) {
        uint32_t zl = zcompress(g_src, SRC_LEN, 9, Z_DEFAULT_STRATEGY);
        g_z[(trial * 7919u) % zl] ^= (uint8_t)(1u + trial);
        int32_t got = inflate_chunked(g_z, zl, 240u, (uint32_t)sizeof g_out);
        bad += (got != (int32_t)SRC_LEN || memcmp(g_out, g_src, SRC_LEN) != 0) ? 1u : 0u;
    }
    CHECK(bad == 40u);                                  /* every corruption noticed, no crash */
    rn_inflate_init(&g_inf);
    uint8_t big[RN_INFLATE_IN_CAP + 1u];
    (void)memset(big, 0, sizeof big);
    CHECK(rn_inflate_feed(&g_inf, big, (uint32_t)sizeof big) == RN_ERR_FULL);
}

void test_inflate(void)
{
    (void)printf("inflate\n");
    test_block_types();
    test_corrupt();
}
