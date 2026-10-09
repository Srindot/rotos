# ROTOS

Motorcycle navigation instrument: phone does the thinking, ROTOS draws.
This repo is the software digital twin. No hardware needed yet.

```
 phone/  (Python, prototype of the Android app)
   Google route ──┐
                  ├─ fuse + quantize ──► out/route.rmap   map tiles (4-bit classes) + route + maneuvers
   OSM raster  ───┘   + OSM bridges ─► flyover hints per maneuver
   simulated GPS ─ localization ───────► out/ride.rfeed   NAVIGATION_STATE + PHONE_STATUS + WEATHER packets
   Open-Meteo, sun position ───────────┘

 core/   (C++, portable: this goes on the ESP32 unchanged)
   protocol ► tracker ► map_view + hud ► canvas (4 bpp) ► ili9341 driver ► SpiBus
                                                                             │
 host/   (desktop glue: plays the phone from files, SpiBus over TCP) ───────┘
                                                                             │ virtual SPI
 screen/ (standalone ILI9341 240x320 emulator, knows nothing about ROTOS) ◄─┘
```

## Run

```sh
python3 phone/build.py                      # fetch OSM, fuse, quantize -> out/
cmake -S . -B build && cmake --build build -j
./build/rotos_screen &                      # the "TFT"  (G grid, S screenshot)
./build/rotos_sim                           # the "ESP32" (--theme auto|day|night, --glass 70, --north-up, --speed 4)
```

What you'll see: boot → dashboard (mascot, clock, weather, phone status) → navigation
(flyover hints, glass bars, auto day/night) → ride summary card → dashboard again.

Headless (no window): `./build/rotos_sim --headless --snap 12000,300000` writes PPM frames to `out/snaps/`.

## Hardware-limits-as-features

| Limit | Trick |
|---|---|
| ~400 KB RAM | 4-bit indexed framebuffer: 37.5 KiB, not 150 KiB |
| 16-colour palette | map stored as *semantic* classes; themes recolour for free |
| SPI bandwidth (5 MB/s @ 40 MHz) | 16-row bands, unchanged bands are skipped |
| no FPU (ESP32-C3) | map rotation with 16.16 fixed-point steps |
| 1 Hz phone fixes, lost packets | dead-reckoning + easing in `tracker.cpp` |
| low resolution | hand-drawn 5x7 font, integer scaling, 1-bit icons |
| no alpha channel | glass panels via a second pre-blended palette inside windows (SNES/GBA colour math) |
| day vs night | phone sends a NIGHT flag from sun elevation; ROTOS crossfades the 16 palette entries (NES fade) |

## Map data

Route: Google Routes API v2 format (`phone/data/google_route_mock.json`, generated from OSRM
by `phone/tools/make_mock_google.py`). Map: © OpenStreetMap contributors, standard tiles,
cached in `phone/cache/`. Respect the [tile usage policy](https://operations.osmfoundation.org/policies/tiles/).
