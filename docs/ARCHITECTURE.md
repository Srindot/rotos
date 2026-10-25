# ROTOS architecture

The phone does the thinking; ROTOS draws. Everything is split so that one
piece of code runs everywhere it's needed:

```
                       core/  (C11, NASA style, no malloc, no OS)
          ┌──────────────── one library, three users ─────────────────┐
          │ route model · map matching · ETA · flyover hints · sun    │
          │ map raster → map classes · map pack writer/reader · RLE   │
          │ link protocol · tracker (dead reckoning) · ride simulator │
          └──────┬─────────────────────┬──────────────────────┬───────┘
                 │ JNI                 │ ctypes                │ compiled in
                 ▼                     ▼                       ▼
   android/ Android app      sim/phone/ virtual phone     firmware/app/ + ui/firmware/
   Google route, GPS,        same job on the desktop,     state machine, HUD,
   Google map tiles,         makes .rfeed captures or     4-bit canvas, map view,
   weather, BLE link         talks TCP live               map receiver, buttons
                 │                     │                       │
                 │ BLE GATT            │ file / TCP            │ hal_*.h (link time)
                 └──────────┬──────────┘           ┌───────────┴───────────┐
                            ▼                      ▼                       ▼
                  phone ⇄ ROTOS protocol    firmware/hal/esp/       sim/host/
                  (docs/PROTOCOL.md)         ESP32: SPI+DMA, BLE,    desktop: TCP to the
                                             flash, GPIO, ADC        screen twin, files
                                                                       │ virtual SPI
                                                                       ▼
                                                                 sim/screen/ ILI9341 twin
```

## Directories

| Dir | What | Language |
|---|---|---|
| `core/` | All algorithms, shared by phone, simulator and device. `include/rn/*.h` (one header per module); `src/` grouped by task: `base/` (assert, CRC, wire, text, RLE), `geo/` (geodesy, polyline, sun), `route/` (route model, maneuvers, matcher, tracker, engine, ride simulator), `map/` (map pack, corridor, flyovers, mapgen), `link/` (protocol), `ffi/` (flat API for JNI / ctypes); `tests/` (unit tests under ASan/UBSan); `python/rotos_nav.py` (ctypes binding of `rn_ffi.h`); `sources.cmake` (the one source list every build includes). | C11 |
| `firmware/` | ESP-IDF project. `app/` is the portable ROTOS logic (also compiled into the simulator), grouped as `src/core` (state machine, events, settings), `src/link` (link, map receiver), `src/ride`, `src/phone` (status, calls), `src/device` (buttons, backlight, panel); `hal/include/` the hardware seam; `hal/esp/` the ESP32 implementation; `main/main.c` brings the hardware up in order and runs the frame loop; `scripts/` build / flash / hwtest in Docker; `sources.cmake` lists the portable sources for the firmware and the simulator. | C11 |
| `ui/` | Everything drawn. `ui/firmware/`: ROTOS screens in C (`draw/` canvas, font, icons, sprites, widgets; `theme/`; `views/` map and nav views, presenter; `screens/` one file per screen). `ui/android/`: the app's `ui` package (MainActivity, RideScreen, UsageCard, ...) and `res/` (layouts, Catppuccin theme), added to the Gradle source set. | C11, Kotlin |
| `sim/` | `screen/` ILI9341 digital twin (SDL2; keys 1/2/3 = buttons, L = light sensor); `host/` the desktop HAL + `rotos_sim`; `phone/` the virtual phone (Python, all maths through the C library). | C, Python |
| `android/` | Android app (Kotlin + JNI to `core/`), Docker build and emulator. Packages by task: `ride/` (RideService + Dashboard, GuidanceMirror, MapRide, EngineRide, RerouteRules, FrameSender, LinkOpener), `quota/` (UsageGuard), `route/`, `map/`, `mirror/`, `link/`, `nav/`, `call/`, `phone/`, `location/`, `share/`, `net/`, `util/`. | Kotlin |
| `cad/` | Enclosure (yours). | — |
| `docs/` | This documentation. | Markdown |
| `tools/` | `nasa_check.py`: coding-standard checks + cppcheck. | Python |

## Data flow during a ride

Google Maps navigates; the phone mirrors it to ROTOS and adds the map.

1. **Start:** Google Maps starts navigating (the app sees its notification),
   or the rider shares a place from Google Maps to ROTOS.
2. **Turns:** `MapsNotificationListener` reads Google's notification
   (distance, road, arrow icon via `ArrowClassifier`) → GUIDANCE frames.
3. **Route:** for a shared place, Google Routes (`TWO_WHEELER`, alternatives)
   → `RouteMatcher` picks the one Google Maps is navigating → `rn_route`.
   Without a place, the map covers the area around the rider. Every Google
   call is capped by `UsageGuard`; at a cap no new map is built.
4. **Map pack:** `rn_corridor` picks the 32 px cells near the route.
   `rn_mapgen` turns Google tiles (ROTOS-coloured style) into 8 flat map
   classes (classify → majority downsample → inpaint → harmonize roads →
   despeckle → outline; the OSM-only footpath remover is skipped).
   `rn_packwriter` stores the tiles RLE-compressed with the route.
5. **Transfer:** MAP_BEGIN / MAP_CHUNK / MAP_END over BLE with a 40-chunk
   window and CRC-32 (skipped when ROTOS already holds the pack). ROTOS writes
   the pack into a flash partition and reads it **in place** from then on.
6. **Riding:** at 1 Hz, GPS fix → `rn_engine` map matching on Google's line →
   NAV_STATE (free mode: position, heading, place on the line, Google's
   distance left and ETA). `RerouteRules` notice when Google Maps took
   another route and plan it again; leaving the map builds a new area map.
7. **ROTOS:** `rn_tracker` turns 1 Hz packets into smooth 30 fps motion.
   `rt_navview` draws the rotated map from the pack (16.16 fixed point; tiles
   decoded into a 64 KB set-associative cache), the line, the rider and the
   turn HUD into a 4-bit canvas. `rt_present` expands it through the 16-colour
   palette and sends only the 16-row bands that changed, over SPI with DMA.

Demo and sample rides use ROTOS's own route engine instead of Google Maps
(`EngineRide`): the engine's NAV_STATE carries its turns and reroute requests.

## The device application (`firmware/app`)

```
main loop (33 ms):  service_link → service_input → step → render
                         │               │            │        │
                  rt_link (frames)   rt_input     state machine  rt_navview / screens
                  rt_maprx (flash)   (debounce,   rt_themectl    rt_canvas (4 bpp)
                  rt_phone           long press)  rt_ride        rt_present (bands, DMA)
                                                  rt_backlight   rt_ili9341
```

States: **Boot → Waiting → Dashboard → Navigating → Summary (15 s) → Dashboard**.
With Google Maps navigating, the dashboard page becomes the turn screen
(`rt_screen_guide.c`, from GUIDANCE packets).
Waiting (no phone) is an animated idle screen (`rt_screen_idle.c`: a pixel
rider on a scrolling road, stars or sun by theme) with the last ride and the
odometer, which ROTOS keeps in the settings record in flash. While the phone
prepares a route it sends PREP_STATUS and ROTOS shows the progress.
Pages (button A): idle **Dashboard ⇄ Weather**; riding **Map → Dashboard → Weather**
(back to the map automatically after 8 s while moving). An incoming call
(`rt_call`) overlays a popup on any page; while it rings, A and B answer it.
A ride starts when a NAV_STATE arrives whose `route_id` matches the stored map pack.

Memory: framebuffer 38 KB, tile cache 64 KB, SPI band buffers 15 KB, BLE
buffers. The tile cache lives outside `RtApp` and `main.c` provides it:
static on the ESP32-C3/S3, taken from the heap once at boot on the classic
ESP32 (ESP32 DevKit V1), whose static RAM is capped at ~160 KB.
App image ≈ 600 KB of the 1.7 MB partition. The map pack lives in a 2.25 MB
flash partition (routes up to ~50 km).

## Hardware limits as features

| Limit | Trick |
|---|---|
| ~400 KB RAM | 4-bit indexed framebuffer: 37.5 KiB, not 150 KiB; map pack read in place from flash |
| 4 MB flash | map tiles RLE-compressed ~3×, decoded on demand into a tile cache |
| 16-colour palette | the map is stored as *semantic classes*; themes recolour it for free |
| SPI bandwidth (5 MB/s at 40 MHz) | 16-row bands, unchanged bands skipped, DMA overlapped with drawing |
| No FPU (ESP32-C3; the classic ESP32 has one) | map rotation with 16.16 fixed-point steps; floats only per frame, not per pixel |
| 1 Hz phone fixes, lost packets | dead reckoning + easing in `rn_tracker`; route_id guards against wrong maps |
| Low resolution | hand-drawn 5×7 font, integer scaling, 1-bit sprites |
| No alpha channel | glass panels through a second pre-blended palette (SNES/GBA colour math) |
| Day vs night | the phone sends a NIGHT flag from sun elevation; ROTOS crossfades the 16 palette entries (NES fade) |
| Sunlight | automatic backlight from an optional LDR; high-contrast day theme |
