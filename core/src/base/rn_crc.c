/*
 * rn_crc.c - CRC-16/CCITT and CRC-32, bitwise (no tables: 0 bytes of RAM,
 * fast enough for 1 Hz packets and a one-off map transfer).
 */
#include "rn/rn_crc.h"

uint16_t rn_crc16(const uint8_t *data, uint32_t n, uint16_t crc)
{
    uint16_t c = crc;
    if (!RN_ASSERT(data != NULL || n == 0u)) {
        return c;
    }
    for (uint32_t i = 0u; i < n; i++) {
        c ^= (uint16_t)((uint16_t)data[i] << 8);
        for (uint32_t b = 0u; b < 8u; b++) {
            c = ((c & 0x8000u) != 0u) ? (uint16_t)((uint16_t)(c << 1) ^ 0x1021u)
                                      : (uint16_t)(c << 1);
        }
    }
    return c;
}

uint32_t rn_crc32_update(const uint8_t *data, uint32_t n, uint32_t crc)
{
    uint32_t c = crc;
    if (!RN_ASSERT(data != NULL || n == 0u)) {
        return c;
    }
    for (uint32_t i = 0u; i < n; i++) {
        c ^= (uint32_t)data[i];
        for (uint32_t b = 0u; b < 8u; b++) {
            uint32_t mask = (uint32_t)(-(int32_t)(c & 1u));
            c = (c >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return c;
}

uint32_t rn_crc32_final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFu;
}

uint32_t rn_crc32(const uint8_t *data, uint32_t n)
{
    return rn_crc32_final(rn_crc32_update(data, n, RN_CRC32_INIT));
}
