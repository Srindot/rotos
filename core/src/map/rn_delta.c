/*
 * rn_delta.c - streaming patch decoder (device and phone).
 */
#include "rn/rn_delta.h"

#include <string.h>

#include "rn/rn_wire.h"

void rn_delta_init(RnDelta *d)
{
    if (RN_ASSERT(d != NULL)) {
        (void)memset(d, 0, sizeof *d);
    }
}

bool rn_delta_done(const RnDelta *d)
{
    return RN_ASSERT(d != NULL) && !d->in_op && d->hdr_n == 0u && !d->corrupt;
}

/* Header complete: start the op it describes. */
static void open_op(RnDelta *d, uint32_t base_len)
{
    d->op = d->hdr[0];
    if (d->op == RN_DELTA_LIT) {
        d->left = (uint32_t)d->hdr[1] | ((uint32_t)d->hdr[2] << 8);
    } else if (d->op == RN_DELTA_COPY) {
        d->base_off = (uint32_t)d->hdr[1] | ((uint32_t)d->hdr[2] << 8) | ((uint32_t)d->hdr[3] << 16) |
                      ((uint32_t)d->hdr[4] << 24);
        d->left = (uint32_t)d->hdr[5] | ((uint32_t)d->hdr[6] << 8);
        d->corrupt = d->base_off > base_len || d->left > base_len - d->base_off;
    } else {
        d->corrupt = true;
    }
    d->corrupt = d->corrupt || d->left == 0u;
    d->in_op = !d->corrupt;
    d->hdr_n = 0u;
}

/* Bytes of op header still needed for the op being read. */
static uint32_t header_len(const RnDelta *d)
{
    return (d->hdr_n > 0u && d->hdr[0] == RN_DELTA_COPY) ? 7u : 3u;
}

int32_t rn_delta_run(RnDelta *d, const uint8_t *in, uint32_t n, uint32_t *used, const uint8_t *base,
                     uint32_t base_len, uint8_t *out, uint32_t cap)
{
    if (!RN_ASSERT(d != NULL && used != NULL && out != NULL) || !RN_ASSERT(in != NULL || n == 0u)) {
        return RN_ERR_PARAM;
    }
    uint32_t i = 0u;
    uint32_t k = 0u;
    for (uint32_t guard = 0u; guard < n + cap + 16u && !d->corrupt && k < cap; guard++) {
        if (!d->in_op) {
            if (i >= n) {
                break;
            }
            d->hdr[d->hdr_n++] = in[i++];
            if (d->hdr_n >= header_len(d)) {
                open_op(d, base_len);
            }
            continue;
        }
        uint32_t room = cap - k;
        uint32_t take = (d->left < room) ? d->left : room;
        if (d->op == RN_DELTA_LIT) {
            take = (take < n - i) ? take : n - i;
            if (take == 0u) {
                break;                                /* needs more patch bytes */
            }
            (void)memcpy(&out[k], &in[i], take);
            i += take;
        } else {
            if (base == NULL) {
                d->corrupt = true;
                break;
            }
            (void)memcpy(&out[k], &base[d->base_off], take);
            d->base_off += take;
        }
        k += take;
        d->left -= take;
        d->in_op = d->left > 0u;
    }
    *used = i;
    return d->corrupt ? RN_ERR_FORMAT : (int32_t)k;
}
