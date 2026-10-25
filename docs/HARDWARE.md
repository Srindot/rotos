# ROTOS hardware

A personal build: cheapest parts that do the job, no PCB, no battery. The
bike's USB-C port powers it and it turns on with the ignition.

## Components (the build in hand)

| # | Part | Notes | Approx. price |
|---|---|---|---|
| 1 | **ESP32 DevKit V1** (ESP32-WROOM-32, 30 pins, micro-B) | 2 cores at 240 MHz, 520 KB RAM, 4 MB flash, BLE 4.2, antenna on the board. | have |
| 2 | **2.8" SPI TFT, ILI9341, 240×320, no touch** | Check the listing says ILI9341. An ST7789 panel also works with a different init table in `rt_ili9341.c`. | have |
| 3 | 3 × tactile push buttons | A = next page / answer a call with the SMS reply, B = theme / ignore a call, C = spare. | have |
| 4 | Power cable from the bike's USB-C port | **USB-C to micro-B cable** (best: one plug, nothing to come apart), or a USB-C (plug) → USB-A (socket) OTG adapter plus an A-to-micro-B cable. See Power. | ~₹150 |
| 5 | Breadboard + jumper wires, then perfboard + hookup wire + header pins | Breadboard on the desk; solder for the bike (jumpers shake loose). | ~₹150 |
| 6 | Enclosure + mount | 3D printed (see `cad/`), plus a handlebar/phone mount. Matte anti-glare film over the screen helps in sunlight. The DevKit is about 55 × 28 mm. | — |

No light sensor: the backlight is wired to 3V3 and always at full brightness.

Other boards the firmware builds for, with their own pin map in
`firmware/main/board.h`: Seeed XIAO ESP32-C3 (`firmware/scripts/build.sh esp32c3`, much
smaller, needs its external antenna plugged in) and an ESP32-S3 DevKit
(`firmware/scripts/build.sh esp32s3`).

## Wiring (ESP32 DevKit V1)

Pins are set in `firmware/main/board.h`. GPIO numbers are printed on the
board as `D18`, `D23`, … (some boards print just the number).

| TFT / part | DevKit pin | Purpose |
|---|---|---|
| VCC | VIN (5 V from USB) | Most red ILI9341 modules have a 3.3 V regulator; 5 V in is fine. If yours has no regulator (jumper J1 closed), use 3V3 |
| GND | GND | |
| SCK | GPIO18 | SPI clock |
| SDI (MOSI) | GPIO23 | SPI data |
| CS | GPIO5 | chip select |
| DC (RS) | GPIO16 | data / command |
| RESET | GPIO17 | |
| LED (backlight) | 3V3 | always on |
| SDO (MISO) | — | not connected |
| Button A | GPIO32 → GND | internal pull-up, no resistor needed |
| Button B | GPIO33 → GND | |
| Button C | GPIO25 → GND | |

SCK, MOSI and CS are the ESP32's native VSPI pins, which is what lets the SPI
bus run at 40 MHz. Pins to leave alone: GPIO 6–11 (the flash chip), GPIO 0, 2,
12 and 15 (they decide how the chip boots; a button held on one stops it
starting), and GPIO 34–39 (input only, no pull-ups, so no buttons there).

The serial console goes through the board's USB-serial chip on the micro-B
port: CH9102 on the board in use (`/dev/ttyACM0`), CP2102/CH340 on others
(`/dev/ttyUSB0`). The scripts find the port themselves.

**This 2.8" module's controller is wired 320 × 240 (landscape).** The firmware
turns it into the 240 × 320 portrait screen with `BOARD_PANEL_MADCTL 0xE8`
(`firmware/main/board.h`). A different module may need a different value:
run `firmware/scripts/hwtest.sh` and press B until the chart is upright and fills the
glass.

## Power

- The bike's USB-C port gives 5 V to devices that don't negotiate higher
  voltages; ROTOS needs about 1 W (up to ~2 W with BLE transmitting).
- **USB-C to micro-B cable:** the cable has the resistor inside the C plug that
  tells the bike's port to switch on, so it works with no adapter.
- **Or OTG adapter + cable:** USB-C *plug* → USB-A *socket* (OTG) adapter, then
  a USB-A to micro-B cable. The adapter contains the same resistor. A plain
  A-socket-to-C-socket coupler without it won't switch the port on.
- **Vibration:** micro-B is the weak point. Zip-tie or hot-glue the cable to the
  board or enclosure. Tape or heat-shrink any adapter joint.
- **Cleanest permanent option:** a USB-C breakout board inside the enclosure
  feeding the DevKit's VIN pin, with an on/off switch in that line if the
  bike's port stays powered with the ignition off. Don't feed VIN and the
  micro-B port at the same time.

## Lessons from the first bring-up

- Reversed VCC/GND on the TFT made it smoke briefly; it survived. Check
  polarity twice before the first power-on.
- The micro-B USB link is touchy: a slightly loose plug drops the data lines
  (`error -71` in the kernel log) while power still arrives. Replug in another
  port and keep the board still while flashing.

## Never connected

ROTOS never connects to the motorcycle's ECU, CAN bus or wiring harness. It only
uses USB power. The instrument cluster already shows fuel and odometer.

## Bring-up checklist

1. Breadboard the TFT, build and flash. The boot logo should slide in,
   followed by "WAITING FOR PHONE".
   For the DevKit: `firmware/scripts/hwtest.sh` first (screen, orientation, buttons),
   then `firmware/scripts/build.sh esp32` and `firmware/scripts/flash.sh esp32`.
   If flashing doesn't start, hold BOOT while it says "Connecting...".
2. Add the buttons: B should cycle the theme (auto → day → night).
3. Install the Android app (`android/`), pick **BLE** and plan a route. The map
   transfers ("LOADING MAP"), then navigation starts.
4. Solder onto perfboard, box it, mount it, and test in noon sunlight with
   both themes.
