/*
 * rn_packwriter.h - builds a map pack (format in rn_mappack.h) into a
 * caller-provided buffer.
 *
 *   rn_packw_begin(&w, buf, cap, &corridor)   slots = kept cells, row-major
 *   rn_packw_add_tile(&w, cx, cy, classes)    any order, each kept cell once
 *   rn_packw_finish(&w, &route, &len)         route + maneuvers + header
 *
 * Kept cells that never receive a tile are stored as plain land.
 */
#ifndef RN_PACKWRITER_H
#define RN_PACKWRITER_H

#include "rn/rn_corridor.h"
#include "rn/rn_mappack.h"
#include "rn/rn_route.h"

typedef struct {
    uint8_t *out;
    uint32_t cap;
    const RnCorridor *corridor;
    uint32_t data_pos;      /* next free byte of tile data (absolute)   */
    uint32_t n_slots;
    uint32_t n_added;
    bool failed;
    RnPackHeader h;
} RnPackWriter;

rn_status rn_packw_begin(RnPackWriter *w, uint8_t *out, uint32_t cap, const RnCorridor *c);
rn_status rn_packw_add_tile(RnPackWriter *w, uint32_t cx, uint32_t cy, const uint8_t *classes);
rn_status rn_packw_finish(RnPackWriter *w, const RnRoute *r, uint32_t *out_len);

#endif /* RN_PACKWRITER_H */
