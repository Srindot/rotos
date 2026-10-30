/*
 * hal_storage.h - persistent storage: map packs (a raw flash partition on
 * the ESP32, RAM + a file in the simulator) and a small settings record.
 *
 * Two slots: a new pack is written into the slot the current one does not
 * use, so the current pack stays readable (and drawn) during a transfer and
 * is replaced only by a successful commit. hal_storage_map() points into
 * memory-mapped flash; the pointer changes on commit (and on a begin for a
 * pack larger than one slot, which drops the current pack).
 */
#ifndef HAL_STORAGE_H
#define HAL_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#define HAL_SETTINGS_SIZE 16u

bool hal_storage_init(void);
/* Largest map pack the slot can hold, bytes. */
uint32_t hal_storage_map_capacity(void);
/* Start writing a new pack of `len` bytes into the free slot. */
bool hal_storage_map_begin(uint32_t len);
/* Write bytes at offset (offsets arrive in order). */
bool hal_storage_map_write(uint32_t offset, const uint8_t *bytes, uint32_t n);
/* Read back bytes of the pack being written (to check what really reached flash). */
bool hal_storage_map_read(uint32_t offset, uint8_t *out, uint32_t n);
/* Make the new pack current (len bytes, already CRC-checked by the caller). */
bool hal_storage_map_commit(uint32_t len);
/* Drop the pack being written (failed transfer); the current pack stays. */
void hal_storage_map_abort(void);
/* The current pack, or NULL; *len receives its length. */
const uint8_t *hal_storage_map(uint32_t *len);
/* Housekeeping, once per frame while no pack is being written: prepares the
 * free slot for the next transfer a little at a time. */
void hal_storage_service(void);

bool hal_storage_settings_load(uint8_t *out, uint32_t n);
bool hal_storage_settings_save(const uint8_t *in, uint32_t n);

/* The ride in progress (rt_resume), so a power cut at a signal does not end
 * it: saved every ~30 s while riding, cleared when the ride ends. */
bool hal_storage_ride_load(uint8_t *out, uint32_t n);
bool hal_storage_ride_save(const uint8_t *in, uint32_t n);
void hal_storage_ride_clear(void);

#endif /* HAL_STORAGE_H */
