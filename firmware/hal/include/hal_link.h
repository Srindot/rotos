/*
 * hal_link.h - the phone link: a reliable-enough byte pipe in both directions.
 *
 * Implementations:
 *   firmware/hal/esp/hal_link_esp.c   BLE GATT (Nordic-UART layout), NimBLE
 *   sim/host/hal_link_sim.c           .rfeed replay or a TCP server
 *
 * Bytes may arrive in arbitrary pieces; framing is the protocol's job.
 */
#ifndef HAL_LINK_H
#define HAL_LINK_H

#include <stdbool.h>
#include <stdint.h>

bool hal_link_init(void);
/* Copy up to cap received bytes into buf; returns the count (0 = none). */
uint32_t hal_link_read(uint8_t *buf, uint32_t cap);
/* Queue bytes to the phone; false if not connected or the queue is full. */
bool hal_link_write(const uint8_t *bytes, uint32_t n);
bool hal_link_connected(void);

#endif /* HAL_LINK_H */
