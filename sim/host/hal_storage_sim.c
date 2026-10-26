/*
 * hal_storage_sim.c - the "flash" of the simulated device: two map pack
 * slots in RAM, the current pack mirrored to <state_dir>/map.rmap on commit, the
 * settings record to <state_dir>/settings.bin, so a restarted simulator
 * keeps its map like a real device keeps its flash.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "hal_log.h"
#include "hal_storage.h"
#include "sim_config.h"

#define AREA_MAX (16u * 1024u * 1024u)
#define NO_WRITE 0xFFFFFFFFu

/* Two slots like the device: the current pack stays readable while the
 * next one is written into the other slot. */
static uint8_t s_area[AREA_MAX];
static uint32_t s_cur_slot;
static uint32_t s_len;            /* current pack length, 0 = none */
static uint32_t s_new_slot;
static uint32_t s_writing = NO_WRITE;

static uint32_t slot_size(void)
{
    return hal_storage_map_capacity() / 2u;
}

static void path_of(char *out, size_t cap, const char *name)
{
    (void)snprintf(out, cap, "%s/%s", sim_config()->state_dir, name);
}

static uint32_t read_file(const char *name, uint8_t *buf, uint32_t cap)
{
    char p[512];
    path_of(p, sizeof p, name);
    FILE *f = fopen(p, "rb");
    if (f == NULL) {
        return 0u;
    }
    uint32_t n = (uint32_t)fread(buf, 1u, cap, f);
    (void)fclose(f);
    return n;
}

static bool write_file(const char *name, const uint8_t *buf, uint32_t n)
{
    char p[512];
    (void)mkdir(sim_config()->state_dir, 0755);
    path_of(p, sizeof p, name);
    FILE *f = fopen(p, "wb");
    if (f == NULL) {
        return false;
    }
    bool ok = fwrite(buf, 1u, n, f) == n;
    return (fclose(f) == 0) && ok;
}

bool hal_storage_init(void)
{
    s_cur_slot = 0u;
    s_len = read_file("map.rmap", s_area, hal_storage_map_capacity());
    hal_log("flash", "stored map pack bytes", (int32_t)s_len);
    return true;
}

uint32_t hal_storage_map_capacity(void)
{
    uint32_t cap = sim_config()->map_capacity;
    return (cap > AREA_MAX) ? AREA_MAX : cap;
}

bool hal_storage_map_begin(uint32_t len)
{
    s_writing = NO_WRITE;
    if (len == 0u || len > hal_storage_map_capacity()) {
        return false;
    }
    if (len > slot_size() || s_len > slot_size()) {   /* needs (or held) the whole area */
        s_len = 0u;
        s_new_slot = 0u;
    } else {
        s_new_slot = (s_len > 0u && s_cur_slot == 0u) ? 1u : 0u;
    }
    (void)memset(&s_area[s_new_slot * slot_size()], 0xFF, len);
    s_writing = len;
    return true;
}

bool hal_storage_map_write(uint32_t offset, const uint8_t *bytes, uint32_t n)
{
    if (bytes == NULL || s_writing == NO_WRITE || offset + n > s_writing) {
        return false;
    }
    (void)memcpy(&s_area[s_new_slot * slot_size() + offset], bytes, n);
    return true;
}

bool hal_storage_map_read(uint32_t offset, uint8_t *out, uint32_t n)
{
    if (out == NULL || s_writing == NO_WRITE || offset + n > s_writing) {
        return false;
    }
    (void)memcpy(out, &s_area[s_new_slot * slot_size() + offset], n);
    return true;
}

bool hal_storage_map_commit(uint32_t len)
{
    if (s_writing == NO_WRITE || len != s_writing) {
        return false;
    }
    s_writing = NO_WRITE;
    s_cur_slot = s_new_slot;
    s_len = len;
    if (!write_file("map.rmap", &s_area[s_cur_slot * slot_size()], len)) {
        hal_log("flash", "could not persist map pack", 0);
    }
    return true;
}

void hal_storage_map_abort(void)
{
    s_writing = NO_WRITE;
}

const uint8_t *hal_storage_map(uint32_t *len)
{
    if (len == NULL) {
        return NULL;
    }
    *len = s_len;
    return (s_len > 0u) ? &s_area[s_cur_slot * slot_size()] : NULL;
}

void hal_storage_service(void)
{
    /* RAM slots need no erasing */
}

bool hal_storage_settings_load(uint8_t *out, uint32_t n)
{
    return out != NULL && read_file("settings.bin", out, n) == n;
}

bool hal_storage_settings_save(const uint8_t *in, uint32_t n)
{
    return in != NULL && write_file("settings.bin", in, n);
}
