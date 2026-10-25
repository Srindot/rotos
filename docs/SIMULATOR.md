# Running the simulator

The simulator runs the **real firmware application** (`firmware/app`) on
the desktop. Only the hardware layer is swapped (`sim/host`): pixels go to a
pixel-exact ILI9341 twin over virtual SPI, the phone link is a file replay or
a TCP socket, and flash is a directory.

## Requirements

Ubuntu packages: `build-essential cmake libsdl2-dev python3-numpy python3-pil`.

## One command

```sh
sim/run.sh               # build, make a ride with the virtual phone, show it in a window
sim/run.sh --headless    # same, no window: PNG-able PPM frames in out/snaps/
sim/run.sh --live        # wait for a live phone (Android emulator) on TCP 7400
```

What you see: boot logo, then the dashboard (mascot, clock, weather, phone
battery and signal) while "LOADING MAP" fills, then navigation (glass panels,
THEN box, flyover chips, automatic day → night at sunset), then the "RIDE
COMPLETE" card, then the dashboard again.

## Step by step

```sh
cmake -S . -B build/host && cmake --build build/host -j
ctest --test-dir build/host                      # navigation unit tests

python3 sim/phone/rotos_phone.py                 # virtual phone -> out/ride.rfeed, out/route.rmap, out/preview.png
./build/host/sim/rotos_screen &                  # the "TFT"
./build/host/sim/rotos_sim --feed out/ride.rfeed # the "ESP32"
```

### Screen twin keys

| Key | Does |
|---|---|
| 1 / 2 / 3 | buttons A / B / C (hold for a long press). A = next page (dashboard / weather / map); during a call popup A = reply, B = ignore |
| L | ambient light sensor: sun / shade / dark / no sensor |
| G | pixel grid |
| S | screenshot (BMP) |
| Q / Esc | quit |

### `rotos_sim` options

```
--feed FILE          replay a capture        --listen PORT   live phone over TCP
--headless           no twin, virtual clock  --speed X       run the clock faster
--snap MS,...        save frames             --snap-dir DIR
--theme auto|day|night   --north-up   --glass PCT
--press A@MS[:HOLD],...  scripted buttons    --fresh         empty "flash"
--state-dir DIR      simulated flash (out/state: map pack + settings survive restarts)
```

### Virtual phone options

```
python3 sim/phone/rotos_phone.py [--route FILE.json | --osrm FROM TO | --google FROM TO]
      [--start HH:MM | sunset-4] [--date YYYY-MM-DD] [--tz 5.5]
      [--rate 20000]          map transfer speed, bytes/s (BLE ~ 20 KB/s)
      [--offline]             cached tiles/bridges only, fixed weather
      [--google-cap]          phone reports its Google usage cap (ROTOS shows G!)
      [--live HOST:PORT]      talk to rotos_sim --listen instead of writing a file
```

Like the app, the virtual phone connects first and sends PREP_STATUS
(route, flyovers, map blocks n/N, ready) for ~7 s before the map transfer.
The virtual phone also sends an hourly forecast (weather page) and plays two
scripted calls during the ride: "AMMA" at 90 s (it prints the `--press` time to
answer it with A in a replay) and "RAVI OFFICE" at 300 s (rings out). In
`--live` mode it answers ROTOS's CALL_ACTION like the Android app.

`--google` needs `MAPS_API_KEY` in the environment. Without a key, use the
saved sample (`sim/phone/data/google_route_mock.json`) or `--osrm`.

## With the Android app

```sh
sim/run.sh --live                 # ROTOS twin waits on TCP 7400
android/scripts/emulator.sh           # see android/README.md; in the app pick "Simulator (TCP)"
```

The emulator reaches the host at `10.0.2.2:7400`. The app then runs the same
conversation it has with the real device over BLE: map upload with ACKs, then
NAV_STATE at 1 Hz.
