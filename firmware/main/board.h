/*
 * board.h - pin map. Default: Seeed XIAO ESP32-C3 + 2.8" ILI9341 SPI module
 * (docs/HARDWARE.md has the wiring table). The ESP32 DevKit V1 and
 * ESP32-S3 DevKit layouts are selected automatically from the build target.
 * -1 = not connected.
 */
#ifndef BOARD_H
#define BOARD_H

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32
/* ESP32 DevKit V1 (ESP32-WROOM-32, 30 pins). SCK/MOSI/CS are the VSPI
 * IO_MUX pins, so 40 MHz works. Avoid GPIO 6-11 (flash), 0/2/12/15
 * (strapping) and 34-39 (input only, no pull-ups) for buttons. */
#define BOARD_NAME        "ESP32 DevKit V1"
#define BOARD_SPI_HOST    SPI3_HOST
#define BOARD_PIN_SCK     18
#define BOARD_PIN_MOSI    23
#define BOARD_PIN_CS      5
#define BOARD_PIN_DC      16
#define BOARD_PIN_RST     17
#define BOARD_PIN_BL      (-1)   /* backlight wired to 3V3 */
#define BOARD_PIN_BTN_A   32
#define BOARD_PIN_BTN_B   33
#define BOARD_PIN_BTN_C   25
#define BOARD_PIN_LDR     (-1)   /* fit an LDR: set to 34 (ADC1 channel 6) */
#define BOARD_LDR_CHANNEL 6
/* The 2.8" module in use has a 320 x 240 (landscape) controller: row/column
 * exchange + both mirrors gives portrait, top away from the pin header side
 * found in testing (firmware/hwtest, mode 3). */
#define BOARD_PANEL_MADCTL 0xE8
/* Static DRAM is capped at ~160 KB on the classic ESP32: the 64 KB tile
 * cache is taken from the heap once at boot instead. */
#define BOARD_TILE_CACHE_ON_HEAP 1
#elif CONFIG_IDF_TARGET_ESP32S3
#define BOARD_NAME        "ESP32-S3 DevKit"
#define BOARD_SPI_HOST    SPI2_HOST
#define BOARD_PIN_SCK     12
#define BOARD_PIN_MOSI    11
#define BOARD_PIN_CS      10
#define BOARD_PIN_DC      9
#define BOARD_PIN_RST     8
#define BOARD_PIN_BL      7
#define BOARD_PIN_BTN_A   4
#define BOARD_PIN_BTN_B   5
#define BOARD_PIN_BTN_C   6
#define BOARD_PIN_LDR     (-1)   /* fit an LDR: set to 1 (ADC1 channel 0) */
#define BOARD_LDR_CHANNEL 0
#else
#define BOARD_NAME        "XIAO ESP32-C3"
#define BOARD_SPI_HOST    SPI2_HOST
#define BOARD_PIN_SCK     8      /* D8  */
#define BOARD_PIN_MOSI    10     /* D10 */
#define BOARD_PIN_CS      20     /* D7  */
#define BOARD_PIN_DC      21     /* D6  */
#define BOARD_PIN_RST     5      /* D3  */
#define BOARD_PIN_BL      (-1)   /* backlight wired to 3V3 */
#define BOARD_PIN_BTN_A   6      /* D4  */
#define BOARD_PIN_BTN_B   7      /* D5  */
#define BOARD_PIN_BTN_C   4      /* D2  */
#define BOARD_PIN_LDR     (-1)   /* fit an LDR on D1: set to 3 (ADC1 channel 3) */
#define BOARD_LDR_CHANNEL 3
#endif

#ifndef BOARD_PANEL_MADCTL
#define BOARD_PANEL_MADCTL 0x48   /* standard ILI9341 module: portrait, BGR */
#endif

#ifndef BOARD_TILE_CACHE_ON_HEAP
#define BOARD_TILE_CACHE_ON_HEAP 0
#endif

#define BOARD_SPI_HZ       40000000   /* drop to 26 MHz if long wires corrupt pixels */
#define BOARD_SPI_MAX_XFER 8192       /* one 16-row band = 7680 bytes */

#endif /* BOARD_H */
