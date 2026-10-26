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
 * A pack only counts once its slot and length are committed to NVS (one
 * u64, one commit), so a power cut mid-transfer leaves the previous map,
 * never a broken one. Long erases run in 64 KB steps that feed the task
 * watchdog.
 */
#include <string.h>

#include "esp_partition.h"
#include "esp_task_wdt.h"
#include "hal_log.h"
#include "hal_storage.h"
#include "nvs.h"
#include "nvs_flash.h"

#define PART_SUBTYPE  0x40
#define SECTOR        4096u
#define NVS_NS        "rotos"
#define KEY_CUR       "pk_cur"      /* u64: slot << 32 | length, one atomic commit */
#define KEY_SLOT      "pk_slot"     /* before pk_cur: read once, never written again */
#define KEY_LEN       "pk_len"
#define KEY_USED0     "pk_used0"
#define KEY_USED1     "pk_used1"
#define KEY_SETTINGS  "settings"
#define NO_WRITE      0xFFFFFFFFu
#define ERASE_STEP    (64u * 1024u)

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
static bool s_fresh;                /* begun, nothing written yet            */

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

/* The current pack (slot, len; len 0 = none), slot and length in one commit. */
static bool put_current(uint32_t slot, uint32_t len)
{
    return s_nvs_ok && nvs_set_u64(s_nvs, KEY_CUR, ((uint64_t)slot << 32) | (uint64_t)len) == ESP_OK &&
           nvs_commit(s_nvs) == ESP_OK;
}

static void load_current(uint32_t *slot, uint32_t *len)
{
    uint64_t v = 0u;
    if (s_nvs_ok && nvs_get_u64(s_nvs, KEY_CUR, &v) == ESP_OK) {
        *slot = (uint32_t)(v >> 32);
        *len = (uint32_t)(v & 0xFFFFFFFFu);
        return;
    }
    *slot = nvs_get(KEY_SLOT, 0u);                   /* firmware before pk_cur */
    *len = nvs_get(KEY_LEN, 0u);
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

/* Map (slot, len) as the current pack. The new mapping is made before the
 * old one is dropped: if it fails, the current pack stays mapped and valid. */
static bool map_current(uint32_t slot, uint32_t len)
{
    if (len == 0u || s_part == NULL || slot > 1u || slot * s_slot_size + len > s_part->size) {
        return false;
    }
    const void *ptr = NULL;
    esp_partition_mmap_handle_t h;
    if (esp_partition_mmap(s_part, slot * s_slot_size, len, ESP_PARTITION_MMAP_DATA, &ptr, &h) != ESP_OK) {
        return false;
    }
    unmap();
    s_ptr = ptr;
    s_mmap = h;
    s_mapped = true;
    s_cur_slot = slot;
    s_cur_len = len;
    return true;
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
    uint32_t slot = 0u;
    uint32_t len = 0u;
    load_current(&slot, &len);
    (void)map_current(slot, len);
    hal_log("flash", "stored map pack bytes", (int32_t)s_cur_len);
    return ok;
}

uint32_t hal_storage_map_capacity(void)
{
    return (s_part != NULL) ? (uint32_t)s_part->size : 0u;
}

/* Erase [from, to) of the partition, synchronously, in steps that feed the
 * task watchdog (a whole 3 MB partition takes longer than its 15 s). */
static bool erase(uint32_t from, uint32_t to)
{
    if (from >= to) {
        return true;                                  /* nothing dirty: no call (it would be misaligned) */
    }
    uint32_t end = round_up(to);
    for (uint32_t at = from / SECTOR * SECTOR; at < end;) {   /* below `from` is erased already */
        /* to the next 64 KB boundary first: aligned steps get the fast block erase */
        uint32_t room = ERASE_STEP - (at % ERASE_STEP);
        uint32_t n = (end - at < room) ? end - at : room;
        if (esp_partition_erase_range(s_part, at, n) != ESP_OK) {
            return false;
        }
        (void)esp_task_wdt_reset();
        at += n;
    }
    return true;
}

/* A pack bigger than a slot: drop the current one and use the whole partition. */
static bool begin_whole(uint32_t len)
{
    unmap();
    s_cur_len = 0u;
    (void)put_current(0u, 0u);
    s_new_slot = 0u;
    /* [0, len) must be erased; only what was written since each slot was clean is dirty */
    uint32_t dirty0 = (s_used[0] < s_slot_size) ? s_used[0] : s_slot_size;
    uint32_t need1 = len - s_slot_size;
    uint32_t dirty1 = (s_used[1] < need1) ? s_used[1] : need1;
    uint32_t c0 = (s_clean[0] < dirty0) ? s_clean[0] : dirty0;
    uint32_t c1 = (s_clean[1] < dirty1) ? s_clean[1] : dirty1;
    if (!erase(c0, dirty0) || !erase(s_slot_size + c1, s_slot_size + dirty1)) {
        return false;
    }
    set_used(0u, s_slot_size);
    /* never forget dirt further into slot 1 than this pack reaches: a later
     * pack there must find it and erase it first (NOR flash only clears bits) */
    set_used(1u, (s_used[1] > need1) ? s_used[1] : need1);
    s_clean[0] = s_slot_size;                        /* erased: a retried BEGIN needs no erase */
    s_clean[1] = round_up(need1);                    /* (forgotten at the first write) */
    s_fresh = true;
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
            (void)put_current(0u, 0u);
        }
        uint32_t dirty = (s_used[t] < len) ? s_used[t] : len;
        uint32_t base = t * s_slot_size;
        ok = (s_clean[t] >= dirty) || erase(base + s_clean[t], base + dirty);   /* rarely needed */
        s_new_slot = t;
        if (s_used[t] < len) {
            set_used(t, len);
        }
        s_clean[t] = ok ? round_up(len) : 0u;        /* erased up to len until the first write */
        s_fresh = true;
    }
    s_new_len = ok ? len : NO_WRITE;
    return ok;
}

bool hal_storage_map_write(uint32_t offset, const uint8_t *bytes, uint32_t n)
{
    if (s_part == NULL || bytes == NULL || s_new_len == NO_WRITE || offset + n > s_new_len) {
        return false;
    }
    if (s_fresh) {                                   /* written now: no longer known clean */
        s_fresh = false;
        s_clean[s_new_slot] = 0u;
        if (s_new_len > s_slot_size) {
            s_clean[1] = 0u;
        }
    }
    return esp_partition_write(s_part, s_new_slot * s_slot_size + offset, bytes, n) == ESP_OK;
}

bool hal_storage_map_read(uint32_t offset, uint8_t *out, uint32_t n)
{
    if (s_part == NULL || out == NULL || s_new_len == NO_WRITE || offset + n > s_new_len) {
        return false;
    }
    return esp_partition_read(s_part, s_new_slot * s_slot_size + offset, out, n) == ESP_OK;
}

bool hal_storage_map_commit(uint32_t len)
{
    if (s_new_len == NO_WRITE || len != s_new_len) {
        return false;
    }
    uint32_t slot = s_new_slot;
    uint32_t old_slot = s_cur_slot;
    uint32_t old_len = s_cur_len;
    s_new_len = NO_WRITE;
    if (!map_current(slot, len)) {
        return false;                                /* the current pack stays mapped and current */
    }
    if (!put_current(slot, len)) {                   /* NVS still names the old pack: keep using it */
        if (old_len > 0u) {
            (void)map_current(old_slot, old_len);
        } else {
            unmap();
            s_cur_len = 0u;
        }
        hal_log("flash", "could not record the new map: kept the old one", 0);
        return false;
    }
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
        if (esp_partition_erase_range(s_part, f * s_slot_size + s_clean[f], SECTOR) == ESP_OK) {
            s_clean[f] += SECTOR;                    /* only what really got erased counts as clean */
        }
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
