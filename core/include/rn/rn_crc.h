/*
 * rn_crc.h - checksums used on the wire and for map packs.
 *
 *   CRC-16/CCITT-FALSE  poly 0x1021, init 0xFFFF  - every link packet
 *   CRC-32 (IEEE 802.3) poly 0xEDB88320 reflected - whole map pack transfers
 *
 * Both are incremental: pass the previous value to continue a running CRC.
 */
#ifndef RN_CRC_H
#define RN_CRC_H

#include "rn/rn_types.h"

#define RN_CRC16_INIT 0xFFFFu
#define RN_CRC32_INIT 0xFFFFFFFFu

uint16_t rn_crc16(const uint8_t *data, uint32_t n, uint16_t crc);

/* Running CRC-32: start with RN_CRC32_INIT, finish with rn_crc32_final(). */
uint32_t rn_crc32_update(const uint8_t *data, uint32_t n, uint32_t crc);
uint32_t rn_crc32_final(uint32_t crc);
/* One-shot convenience. */
uint32_t rn_crc32(const uint8_t *data, uint32_t n);

#endif /* RN_CRC_H */
