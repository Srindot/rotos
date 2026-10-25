/*
 * hal_storage_esp.c - map packs in the raw "mappack" flash partition (read in
 * place through the MMU), settings and the pack bookkeeping in NVS.
 *
 * The partition is two slots. A new pack is written into the slot the
 * current one does not use, so ROTOS keeps drawing the current map during a
 * transfer; the switch happens on commit, after the CRC check. The free slot
 * is erased a sector at a time in the background (hal_storage_service), so
 * the next MAP_BEGIN finds it ready and answers at once. A pack larger than
 * one slot uses the whole partition (the current one is dropped first).
 *
 * A pack only counts once its slot and length are committed to NVS, so a
 * power cut mid-transfer leaves the previous map, never a broken one.
 */
#include <string.h>

#include "esp_partition.h"
#include "hal_log.h"
#include "hal_storage.h"
#include "nvs.h"
#include "nvs_flash.h"

#define PART_SUBTYPE  0x40
#define SECTOR        4096u
#define NVS_NS        "rotos"
#define KEY_SLOT      "pk_slot"     /* new keys: packs from the old layout are ignored */
#define KEY_LEN       "pk_len"
#define KEY_USED0     "pk_used0"
#define KEY_USED1     "pk_used1"
#define KEY_SETTINGS  "settings"
#define NO_WRITE      0xFFFFFFFFu

static const esp_partition_t *s_part;
static nvs_handle_t s_nvs;
static bool s_nvs_ok;
static const void *s_ptr;
static esp_partition_mmap_handle_t s_mmap;
static bool s_mapped;
static uint32_t s_slot_size;        /* bytes per slot, sector aligned        */
static uint32_t s_cur_slot;         /* slot of the current pack              */
static uint32_t s_cur_len;          /* 0 = no current pack                   */
static uint32_t s_new_slot;         /* slot being written                    */
static uint32_t s_new_len = NO_WRITE;
static uint32_t s_used[2];          /* bytes written since the slot was clean */
static uint32_t s_clean[2];         /* erased prefix while cleaning          */

static uint32_t round_up(uint32_t n)
{
    return (n + SECTOR - 1u) / SECTOR * SECTOR;
}

static void nvs_put(const char *key, uint32_t v)
{
    if (s_nvs_ok) {
        (void)nvs_set_u32(s_nvs, key, v);
        (void)nvs_commit(s_nvs);
    }
}

static uint32_t nvs_get(const char *key, uint32_t fallback)
{
    uint32_t v = fallback;
    if (!s_nvs_ok || nvs_get_u32(s_nvs, key, &v) != ESP_OK) {
        return fallback;
    }
    return v;
}

static void set_used(uint32_t slot, uint32_t used)
{
    s_used[slot] = used;
    nvs_put((slot == 0u) ? KEY_USED0 : KEY_USED1, used);
}

static void unmap(void)
{
    if (s_mapped) {
        esp_partition_munmap(s_mmap);
        s_mapped = false;
        s_ptr = NULL;
    }
}

/* Map the current pack (slot, len); false leaves "no pack". */
static bool map_current(uint32_t slot, uint32_t len)
{
    unmap();
    s_cur_len = 0u;
    if (len == 0u || s_part == NULL || slot > 1u || slot * s_slot_size + len > s_part->size) {
        return false;
    }
    s_mapped = esp_partition_mmap(s_part, slot * s_slot_size, len, ESP_PARTITION_MMAP_DATA, &s_ptr,
                                  &s_mmap) == ESP_OK;
    s_cur_slot = slot;
    s_cur_len = s_mapped ? len : 0u;
    return s_mapped;
}

static bool init_nvs(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        (void)nvs_flash_erase();
        e = nvs_flash_init();
    }
    s_nvs_ok = e == ESP_OK && nvs_open(NVS_NS, NVS_READWRITE, &s_nvs) == ESP_OK;
    return s_nvs_ok;
}

bool hal_storage_init(void)
{
    bool ok = init_nvs();
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)PART_SUBTYPE, "mappack");
    if (s_part == NULL) {
        hal_log("flash", "no mappack partition", 0);
        return false;
    }
    s_slot_size = (uint32_t)s_part->size / 2u / SECTOR * SECTOR;
    s_used[0] = nvs_get(KEY_USED0, s_slot_size);     /* unknown = assume dirty */
    s_used[1] = nvs_get(KEY_USED1, s_slot_size);
    (void)map_current(nvs_get(KEY_SLOT, 0u), nvs_get(KEY_LEN, 0u));
    hal_log("flash", "stored map pack bytes", (int32_t)s_cur_len);
    return ok;
}

uint32_t hal_storage_map_capacity(void)
{
    return (s_part != NULL) ? (uint32_t)s_part->size : 0u;
}

/* Erase [from, to) of the partition, synchronously. */
static bool erase(uint32_t from, uint32_t to)
{
    return (to <= from) || esp_partition_erase_range(s_part, from, round_up(to) - from) == ESP_OK;
}

/* A pack bigger than a slot: drop the current one and use the whole partition. */
static bool begin_whole(uint32_t len)
{
    unmap();
    s_cur_len = 0u;
    nvs_put(KEY_LEN, 0u);
    s_new_slot = 0u;
    if (!erase(0u, len)) {
        return false;
    }
    set_used(0u, s_slot_size);
    set_used(1u, len - s_slot_size);
    s_clean[0] = 0u;
    s_clean[1] = 0u;
    return true;
}

bool hal_storage_map_begin(uint32_t len)
{
    s_new_len = NO_WRITE;
    if (s_part == NULL || len == 0u || len > s_part->size) {
        return false;
    }
    bool ok;
    if (len > s_slot_size) {
        ok = begin_whole(len);
    } else {
        uint32_t t = (s_cur_len > 0u && s_cur_len <= s_slot_size && s_cur_slot == 0u) ? 1u : 0u;
        if (s_cur_len > s_slot_size) {               /* a whole-partition pack: replaced */
            unmap();
            s_cur_len = 0u;
            nvs_put(KEY_LEN, 0u);
        }
        uint32_t dirty = (s_used[t] < len) ? s_used[t] : len;
        uint32_t base = t * s_slot_size;
        ok = (s_clean[t] >= dirty) || erase(base + s_clean[t], base + dirty);   /* rarely needed */
        s_new_slot = t;
        s_clean[t] = 0u;
        if (s_used[t] < len) {
            set_used(t, len);
        }
    }
    s_new_len = ok ? len : NO_WRITE;
    return ok;
}

bool hal_storage_map_write(uint32_t offset, const uint8_t *bytes, uint32_t n)
{
    if (s_part == NULL || bytes == NULL || s_new_len == NO_WRITE || offset + n > s_new_len) {
        return false;
    }
    return esp_partition_write(s_part, s_new_slot * s_slot_size + offset, bytes, n) == ESP_OK;
}

bool hal_storage_map_commit(uint32_t len)
{
    if (s_new_len == NO_WRITE || len != s_new_len) {
        return false;
    }
    uint32_t slot = s_new_slot;
    s_new_len = NO_WRITE;
    if (!map_current(slot, len)) {
        nvs_put(KEY_LEN, 0u);
        return false;
    }
    nvs_put(KEY_SLOT, slot);
    nvs_put(KEY_LEN, len);
    s_clean[slot ^ 1u] = 0u;                         /* the old slot gets cleaned next */
    return true;
}

void hal_storage_map_abort(void)
{
    s_new_len = NO_WRITE;                            /* its slot is cleaned in the background */
}

const uint8_t *hal_storage_map(uint32_t *len)
{
    if (len == NULL) {
        return NULL;
    }
    *len = s_cur_len;
    return s_mapped ? (const uint8_t *)s_ptr : NULL;
}

/* One sector of the free slot per call: a short stall, spread over frames. */
void hal_storage_service(void)
{
    if (s_part == NULL || s_new_len != NO_WRITE || s_cur_len > s_slot_size) {
        return;                                      /* writing, or no free slot */
    }
    for (uint32_t k = 0u; k < 2u; k++) {
        uint32_t f = (s_cur_len > 0u) ? (s_cur_slot ^ 1u) : k;
        if (s_used[f] == 0u) {
            continue;
        }
        if (s_clean[f] >= s_used[f]) {
            s_clean[f] = 0u;
            set_used(f, 0u);
            return;
        }
        (void)esp_partition_erase_range(s_part, f * s_slot_size + s_clean[f], SECTOR);
        s_clean[f] += SECTOR;
        return;
    }
}

bool hal_storage_settings_load(uint8_t *out, uint32_t n)
{
    size_t got = n;
    return s_nvs_ok && out != NULL && nvs_get_blob(s_nvs, KEY_SETTINGS, out, &got) == ESP_OK && got == n;
}

bool hal_storage_settings_save(const uint8_t *in, uint32_t n)
{
    return s_nvs_ok && in != NULL && nvs_set_blob(s_nvs, KEY_SETTINGS, in, n) == ESP_OK &&
           nvs_commit(s_nvs) == ESP_OK;
}
