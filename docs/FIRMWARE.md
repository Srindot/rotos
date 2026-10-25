# Building and flashing the firmware

The firmware is an ESP-IDF v5.3 project in `firmware/`. Docker provides the
toolchain, so nothing needs installing.

```sh
firmware/scripts/build.sh esp32           # ESP32 DevKit V1  -> build/esp32/rotos_firmware.bin
firmware/scripts/flash.sh esp32                  # flash + serial monitor (Ctrl+] to exit); port found automatically

firmware/scripts/build.sh                 # ESP32-C3 (XIAO)  -> build/esp32c3/rotos_firmware.bin
firmware/scripts/build.sh esp32s3         # ESP32-S3 DevKit  -> build/esp32s3/rotos_firmware.bin
firmware/scripts/flash.sh esp32c3 /dev/ttyACM0
```

DevKit V1: if flashing hangs at "Connecting...", hold BOOT until it starts.
The serial port needs your user in the `dialout` group. The DevKit in use
has a CH9102 USB chip, which shows up as `/dev/ttyACM0` (CP2102/CH340 boards: `/dev/ttyUSB0`). The first flash of a
XIAO may need boot mode: hold BOOT, tap RESET, release BOOT.

## Hardware test first

`firmware/hwtest/` is a small separate firmware that checks the wiring with
the same screen and button drivers, no phone needed:

```sh
firmware/scripts/hwtest.sh                  # ESP32 DevKit V1: build, flash, serial monitor
firmware/scripts/hwtest.sh esp32 build      # build only
```

The screen shows an orientation chart (four named corner quadrants, a 40 px
grid, an UP arrow, the MADCTL mode; B = next of 8 modes), and A switches to a
test page: colour bars, a border on all four edges, and boxes A/B/C that turn
green while the button is held, with a press count. The mode that is upright
and fills the glass goes into `BOARD_PANEL_MADCTL` in `firmware/main/board.h`. The serial log prints every press and release, plus FPS and free
heap every 5 s.

## Layout

```
firmware/
  main/main.c        start-up order and the frame loop
  main/board.h       pin map per target (ESP32 DevKit V1, XIAO ESP32-C3, ESP32-S3 DevKit)
  app/               portable ROTOS application (also runs in the simulator)
  hal/include/       hal_display / hal_link / hal_storage / hal_input / hal_time / hal_log
  hal/esp/           ESP32 implementations: SPI2 + DMA, NimBLE GATT, flash partition + NVS, GPIO + ADC
  partitions.csv     nvs | factory app 1.7 MB | mappack 2.25 MB (no OTA by design)
  sdkconfig.defaults console on USB-Serial-JTAG, NimBLE peripheral, 1 kHz tick
  sdkconfig.defaults.esp32  classic ESP32: 240 MHz, UART console, BLE-only
                            controller, draw loop on core 1 (BLE on core 0)
```

## Start-up order (`main.c`)

0. tile cache (64 KB): static, or on the classic ESP32 taken from the heap
   once here, before BLE starts (its static RAM is capped at ~160 KB)
1. `hal_storage_init`: NVS and the `mappack` partition; a committed map pack is memory-mapped
2. `hal_display_init`: SPI bus at 40 MHz with DMA, D/C and RST pins, backlight PWM
3. `hal_input_init`: buttons (pull-ups) and the optional LDR on the ADC
4. `hal_link_init`: NimBLE, the GATT service, advertising as `ROTOS`
5. `rt_app_init`: panel init sequence, settings, state machine

Then, every 33 ms: link → input → step → render.

## Measured build (ESP-IDF 5.3.1)

| | ESP32 DevKit V1 | ESP32-C3 | ESP32-S3 |
|---|---|---|---|
| App image | 599 KB (65 % of partition free) | 607 KB (65 % free) | 595 KB (66 % free) |
| Static DRAM | 95 KB of 125 KB (+ 64 KB tile cache on the heap) | 233 KB of 321 KB (tile cache included) | — |

The ESP32 build also boots in Espressif's QEMU through storage, display and
buttons; QEMU has no Bluetooth radio, so BLE is only testable on the board.
The serial log prints `free heap` after BLE starts.

## Verified on hardware

ESP32 DevKit V1 (2026-10-23): boots, screen at 40 MHz upright and full-screen
(`BOARD_PANEL_MADCTL 0xE8`), all three buttons, BLE advertising as `ROTOS`,
72 KB heap free after BLE starts. Not yet tested: a phone connection, the map
transfer and a ride. Things to watch:

- SPI at 40 MHz over jumper wires can corrupt pixels; drop `BOARD_SPI_HZ` to 26 MHz.
- Orientation and red/blue order are one value per board, `BOARD_PANEL_MADCTL`
  in `firmware/main/board.h`; find it with `firmware/scripts/hwtest.sh`. The 2.8" module
  in use has a 320 × 240 (landscape) controller, so it needs the row/column
  exchange bit (0xE8).
- Watch the serial log for `ble MTU 247` when the phone connects; a smaller
  MTU only slows the map transfer down.
