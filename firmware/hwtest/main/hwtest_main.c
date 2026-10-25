/*
 * hwtest_main.c - ROTOS hardware test entry point.
 *
 *   1. log     chip, cores, flash size, pin map
 *   2. display SPI bus + ILI9341 init (same code as the real firmware)
 *   3. input   buttons with internal pull-ups
 *   4. clear   whole panel memory black, so any part the 240 x 320 frame
 *              does not reach stays black instead of showing noise
 *   loop: poll buttons -> draw chart or test page (button A switches)
 *         -> send it in the current orientation (B = next of 8 modes)
 *         serial: every press/release, and FPS + free heap every 5 s
 */
#include <stdint.h>

#include "board.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "hal_display.h"
#include "hal_input.h"
#include "hal_log.h"
#include "hal_time.h"
#include "ht_buttons.h"
#include "ht_panel.h"
#include "ht_palette.h"
#include "ht_screen.h"
#include "rt_canvas.h"
#include "rt_config.h"
#include "rt_ili9341.h"
#include "rt_present.h"

#define REPORT_MS     5000u

static RtCanvas g_canvas;        /* 38 KB: static, never on the stack */
static RtPresenter g_presenter;

static void log_board(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash = 0u;
    (void)esp_flash_get_size(NULL, &flash);
    hal_log("hw", "ROTOS hardware test on " BOARD_NAME ", cores", chip.cores);
    hal_log("hw", "chip revision", chip.revision);
    hal_log("hw", "flash MB", (int32_t)(flash / (1024u * 1024u)));
    hal_log("hw", "TFT SCK  GPIO", BOARD_PIN_SCK);
    hal_log("hw", "TFT MOSI GPIO", BOARD_PIN_MOSI);
    hal_log("hw", "TFT CS   GPIO", BOARD_PIN_CS);
    hal_log("hw", "TFT DC   GPIO", BOARD_PIN_DC);
    hal_log("hw", "TFT RST  GPIO", BOARD_PIN_RST);
    hal_log("hw", "button A GPIO", BOARD_PIN_BTN_A);
    hal_log("hw", "button B GPIO", BOARD_PIN_BTN_B);
    hal_log("hw", "button C GPIO", BOARD_PIN_BTN_C);
}

static void report(uint32_t frames, uint32_t window_ms)
{
    hal_log("hw", "FPS", (int32_t)(frames * 1000u / window_ms));
    hal_log("hw", "free heap, bytes", (int32_t)esp_get_free_heap_size());
}

void app_main(void)
{
    log_board();
    bool lcd = hal_display_init();
    hal_log("hw", lcd ? "display SPI ok" : "display SPI FAILED (check board.h pins)", lcd ? 1 : 0);
    bool btn = hal_input_init();
    hal_log("hw", btn ? "buttons ok" : "buttons FAILED", btn ? 1 : 0);
    rt_ili9341_init();
    rt_present_init(&g_presenter);
    HtButtons buttons;
    ht_buttons_init(&buttons);
    ht_panel_fill(HT_PALETTE.rgb565[HT_BLACK]);       /* whole panel memory black first */
    uint32_t start = hal_time_ms();
    uint32_t window = start;
    uint32_t frames = 0u;
    uint32_t fps = 0u;
    bool chart = true;
    uint32_t a_presses = buttons.presses[0];
    uint32_t b_presses = buttons.presses[1];
    uint32_t mode = ht_panel_mode_of(hal_display_madctl());   /* this board's setting */
    ht_panel_set_mode(mode);
    for (;;) {                                        /* JPL-R2 exempt: main loop */
        uint32_t now = hal_time_ms();
        ht_buttons_poll(&buttons);
        if (buttons.presses[0] != a_presses) {
            a_presses = buttons.presses[0];
            chart = !chart;
        }
        bool turned = buttons.presses[1] != b_presses;
        if (turned) {
            b_presses = buttons.presses[1];
            mode = (mode + 1u) % HT_MODE_COUNT;
            ht_panel_fill(HT_PALETTE.rgb565[HT_BLACK]);
            ht_panel_set_mode(mode);
            hal_log("lcd", "orientation mode", (int32_t)mode);
            hal_log("lcd", "MADCTL", ht_panel_madctl(mode));
        }
        if (chart) {
            ht_screen_chart(&g_canvas, mode, ht_panel_madctl(mode));
        } else {
            ht_screen_test(&g_canvas, &buttons, (now - start) / 1000u, fps);
        }
        ht_panel_push(&g_presenter, &g_canvas, &HT_PALETTE, mode, turned);
        frames++;
        if (now - window >= REPORT_MS) {
            fps = frames * 1000u / (now - window);
            report(frames, now - window);
            frames = 0u;
            window = now;
        }
        hal_time_sleep_until(now + RT_FRAME_MS);
    }
}
