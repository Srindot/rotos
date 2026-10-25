/*
 * rn_wire.h - little-endian byte cursors for wire formats and file formats.
 *
 * Buffers are never cast to structs (alignment, endianness, padding): every
 * field is read or written byte by byte through a bounds-checked cursor. A
 * cursor that runs past its end latches `ok = false` and stays safe to use.
 */
#ifndef RN_WIRE_H
#define RN_WIRE_H

#include "rn/rn_types.h"

typedef struct {
    const uint8_t *buf;
    uint32_t len;
    uint32_t pos;
    bool ok;
} RnReader;

typedef struct {
    uint8_t *buf;
    uint32_t cap;
    uint32_t pos;
    bool ok;
} RnWriter;

RnReader rn_reader(const uint8_t *buf, uint32_t len);
uint8_t rn_rd_u8(RnReader *r);
uint16_t rn_rd_u16(RnReader *r);
uint32_t rn_rd_u32(RnReader *r);
int16_t rn_rd_i16(RnReader *r);
int32_t rn_rd_i32(RnReader *r);
float rn_rd_f32(RnReader *r);
void rn_rd_bytes(RnReader *r, uint8_t *out, uint32_t n);
void rn_rd_skip(RnReader *r, uint32_t n);

RnWriter rn_writer(uint8_t *buf, uint32_t cap);
void rn_wr_u8(RnWriter *w, uint8_t v);
void rn_wr_u16(RnWriter *w, uint16_t v);
void rn_wr_u32(RnWriter *w, uint32_t v);
void rn_wr_i16(RnWriter *w, int16_t v);
void rn_wr_i32(RnWriter *w, int32_t v);
void rn_wr_f32(RnWriter *w, float v);
void rn_wr_bytes(RnWriter *w, const uint8_t *src, uint32_t n);
void rn_wr_zeros(RnWriter *w, uint32_t n);

/* Fixed-position accessors for already-validated buffers (e.g. a map pack
 * in flash). The caller guarantees off + size <= buffer length. */
uint16_t rn_get_u16(const uint8_t *p, uint32_t off);
uint32_t rn_get_u32(const uint8_t *p, uint32_t off);
int32_t rn_get_i32(const uint8_t *p, uint32_t off);
float rn_get_f32(const uint8_t *p, uint32_t off);
void rn_put_u16(uint8_t *p, uint32_t off, uint16_t v);
void rn_put_u32(uint8_t *p, uint32_t off, uint32_t v);

#endif /* RN_WIRE_H */
