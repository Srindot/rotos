/*
 * main.c - ROTOS firmware entry point.
 *
 * Brings the hardware up in a fixed order, then hands every frame to the
 * portable application (firmware/app), exactly like sim/host/main.c:
 *
 *   0. memory    tile cache (static, or boot-time heap on the classic ESP32)
 *   1. storage   flash partition with the map pack, NVS settings
 *   2. display   SPI bus + DMA, panel control pins, backlight
 *   3. input     buttons, ambient light sensor
 *   4. link      BLE GATT service for the phone
 *   5. app       ROTOS (initialises the panel itself)
 *   loop: link -> input -> step -> render -> wait for the next frame
 */
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

#include "board.h"
#include "hal_display.h"
#include "hal_input.h"
#include "hal_link.h"
#include "hal_log.h"
#include "hal_storage.h"
#include "hal_time.h"
#include "rt_app.h"
#include "rt_config.h"

static RtApp g_app;    /* ~115 KB: static, never on the stack */

#if BOARD_TILE_CACHE_ON_HEAP
#include "esp_heap_caps.h"

/* JPL-R3: allocated once, before BLE starts, never freed. */
static RnTileCache *tile_cache_storage(void)
{
    return heap_caps_calloc(1u, sizeof(RnTileCache), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
#else
static RnTileCache g_cache;

static RnTileCache *tile_cache_storage(void)
{
    return &g_cache;
}
#endif

/* Run-time route lines (ROUTE packets): 12 KB, from the heap like the tile cache. */
static RnPackPoint *line_storage(void)
{
    return heap_caps_calloc(RN_ROUTE_MAX_PTS, sizeof(RnPackPoint), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void init_step(bool ok, const char *what)
{
    hal_log("main", what, ok ? 1 : 0);
}

void app_main(void)
{
    hal_log("main", "ROTOS firmware on " BOARD_NAME, 0);
    RnTileCache *cache = tile_cache_storage();
    if (cache == NULL) {
        hal_log("main", "no RAM for the tile cache, bytes", (int32_t)sizeof(RnTileCache));
        return;
    }
    init_step(hal_storage_init(), "storage ready");
    init_step(hal_display_init(), "display ready");
    init_step(hal_input_init(), "input ready");
    init_step(hal_link_init(), "BLE ready");
    hal_log("main", "free heap, bytes", (int32_t)esp_get_free_heap_size());
    hal_log("main", "reset reason (esp_reset_reason_t)", (int32_t)esp_reset_reason());
    RnPackPoint *line = line_storage();
    if (line == NULL) {
        hal_log("main", "no RAM for route lines (reroutes resend the map)", 0);
    }
    rt_app_init(&g_app, cache, line, hal_time_ms());
    g_app.reset_reason = (uint8_t)esp_reset_reason();     /* told to the phone (DEVICE_STATUS) */
    (void)esp_task_wdt_add(NULL);                     /* a stuck loop reboots (sdkconfig: 15 s) */
    for (;;) {                                        /* JPL-R2 exempt: main loop */
        uint32_t now = hal_time_ms();
        (void)esp_task_wdt_reset();
        rt_app_service_link(&g_app, now);
        rt_app_service_input(&g_app, now);
        rt_app_step(&g_app, now);
        rt_app_render(&g_app, now);
        hal_time_sleep_until(now + RT_FRAME_MS);
    }
}
