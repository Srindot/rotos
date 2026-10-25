# ROTOS Android companion app

The phone does the heavy work (routing, map pack building, GPS, map matching)
and streams compact frames to ROTOS over BLE, or over TCP to the simulator.
All navigation maths runs in the shared C library (`../core`) through JNI
(`app/src/main/cpp/rotos_jni.c` -> `NativeNav.kt`). Protocol: `docs/PROTOCOL.md`.

Everything builds and runs in Docker. Nothing needs to be installed on the host
besides Docker, plus `/dev/kvm` access for the emulator.

## Build

```sh
android/scripts/build.sh                 # -> android/out/rotos-debug.apk
```

On first use the script builds the `rotos-apk-build` image (JDK 17, SDK 34,
NDK 26.3, CMake 3.22.1, Gradle 8.7). The repo is mounted, not copied. Manual
equivalent:

```sh
docker build --target build -t rotos-apk-build android/
docker run --rm -v "$PWD:/work" -v rotos-gradle:/root/.gradle -v rotos-android:/root/.android \
    -w /work/android rotos-apk-build gradle --no-daemon assembleDebug
```

The `rotos-android` volume keeps the debug signing key stable, so later builds
install as updates.

## Release build and install on your phone

```sh
android/scripts/build.sh release      # -> android/out/rotos-release.apk (R8-shrunk, ~2.5 MB, signed)
android/scripts/install_phone.sh      # USB: install, grant all permissions, battery unrestricted, launch
android/scripts/build.sh test         # JVM unit tests (rules, parsers, usage caps)
```

The first `build.sh release` creates a signing key in `android/keystore/` plus
`android/keystore.properties` (both gitignored). **Back them up**: an APK signed
with a different key can only be installed after uninstalling the old one.

For USB install, turn on USB debugging once (Settings → About phone → tap
Build number 7 times; Settings → System → Developer options → USB debugging),
plug the phone in and tap Allow. Installing over USB also avoids Play
Protect's "App blocked to protect your device", which stops sideloaded APKs
that ask for call-log / SMS permissions.

## Using it

Google Maps does the navigation (route, turns, voice, rerouting); ROTOS rides
alongside with a map built from Google's tiles, your live position and Google's
next turn. Places are always picked in Google Maps.

1. **Connect to ROTOS** (switch, off by default): links the bike over
   Bluetooth and keeps it linked in the background: clock, weather, battery,
   calls and notifications on ROTOS.
2. **Blue line: Google Maps → place → Share → ROTOS.** From the share the app
   fetches Google's two-wheeler route (Routes API, with alternatives; the one
   closest to what Google Maps navigates is kept), builds the map pack from
   Google tiles, sends map + route to ROTOS (~2-5 s) and opens Google Maps
   navigation there. The C engine map-matches every fix to the line. Leaving
   it, or Google Maps clearly taking another road (distance left or announced
   road disagreeing for 15 s while you move), fetches a new route and resends.
   The shared route is remembered: if the app restarts mid-ride it comes back.
3. **Or just navigate in Google Maps.** ROTOS then builds a 4 × 4 km map around
   you (`autoRideLoop` in RideService) and shows Google's turns over it, with no
   line (Google Maps does not tell other apps the destination). Riding off the
   map rebuilds it around you; Google Maps stopping ends the ride after 45 s.
4. **Stop ride** ends the ride at once; Google's turns stay off ROTOS until that
   Google navigation ends (also across app restarts).

- Notification access is needed once (Google Maps turns and your
  notifications; `install_phone.sh` grants it over USB). The reader parses the
  English notification text (`mirror/GuidanceParser.kt`).
- **Advanced:** test rides (demo / sample switches: ROTOS's own simulated
  route), simulator link, status and log.
- **Log:** `adb shell cat /sdcard/Android/data/dev.rotos.app/files/rotos.log`
  (also release builds; logcat on the phone only keeps minutes). It never
  contains message text.

Test without Google Maps (debug builds only):

```sh
adb shell cmd notification allow_listener dev.rotos.app/dev.rotos.app.mirror.MapsNotificationListener
adb shell am start -n dev.rotos.app/.ui.MainActivity --es link tcp --es host 10.0.2.2 --ei port 7400 --ez connect true
adb shell cmd notification post -t "'250 m'" gmaps-test "'Turn right onto Old Mumbai Hwy · 12 min · 4.2 km · 17:58 ETA'"
```

Theme: Catppuccin Latte by day and Mocha by night, following the phone's
dark mode, matching the ROTOS night palette.

## Google API key

ROTOS gets its routes and maps only from Google. Without a key it still
mirrors Google Maps' turns, but builds no maps.

| API to enable | What ROTOS gets |
|---|---|
| **Routes API** | Google's two-wheeler route (traffic-aware, with alternatives) for the blue line and reroutes |
| **Map Tiles API** | Google's map for the ROTOS screen: 2D roadmap tiles in a custom style painted in the classifier's colours, labels off, thick local roads (`map/GoogleTiles.kt`) |
| **Maps SDK for Android** | the Google map inside the phone app's route card |

Creating it: console.cloud.google.com → new project → billing on (personal
use normally stays inside Google's free monthly usage) → enable the APIs above
→ Credentials → Create API key → Application restrictions: Android apps,
package `dev.rotos.app`, SHA-1 from
`keytool -list -v -keystore android/keystore/rotos-release.jks -alias rotos`
(password in `android/keystore.properties`) → API restrictions: the APIs above.
To use debug builds and the emulator demos with Google too, add the debug
key's SHA-1 as a second Android app entry.
Then:

```sh
echo "MAPS_API_KEY=AIza..." > android/local.properties     # gitignored
android/scripts/build.sh release && android/scripts/install_phone.sh
```

The key is built into the app (`BuildConfig.MAPS_API_KEY`). Never commit it.

## Emulator

```sh
android/scripts/emulator.sh              # window on the host X11 display (DISPLAY, default :0)
android/scripts/emulator.sh --headless   # no window (CI-style)
EMU_TZ=UTC android/scripts/emulator.sh   # timezone (default Asia/Kolkata, to match Open-Meteo local time)
EMU_API=35 android/scripts/emulator.sh  # Android 15 (e.g. Motorola Razr 60) instead of Android 14
android/scripts/install.sh               # install with all permissions granted, launch
android/scripts/screenshot.sh name       # -> android/out/name.png
android/scripts/emulator.sh --stop
```

The container uses `--device /dev/kvm` and `--network host`, so inside the
emulator `10.0.2.2:7400` is the host's `127.0.0.1:7400`.

## Demo ride against a fake or real ROTOS

```sh
android/scripts/emulator.sh --headless
android/scripts/build.sh
SPEEDUP=4 android/scripts/demo.sh        # starts android/tools/fake_rotos.py on :7400
```

`demo.sh` launches the app in demo mode with the bundled sample route
(IIIT Hyderabad -> Durgam Cheruvu, 6.8 km). It then:

- builds the map pack from Google tiles (needs the debug key's SHA-1 on the API key; tiles are cached)
- uploads the pack over TCP with the sliding window
- replays a ride from the library's ride simulator: one fix per simulated
  second, delivered `SPEEDUP` times faster

`fake_rotos.py` checks the map CRC, prints every NAV_STATE / PHONE_STATUS /
WEATHER packet, and exits 6 s after ARRIVED with a PASS/FAIL summary. Output
goes to `android/out/` (`fake_rotos.log`, `received.rmap`, `app_logcat.txt`,
`demo-*.png`).

To test against the real simulator instead:

```sh
cmake -S . -B build/host && cmake --build build/host -j
./build/host/sim/rotos_sim --headless --listen 7400 --fresh --max-s 900 --state-dir /tmp/rotos-state &
SPEEDUP=4 android/scripts/demo.sh --sim
```

Other options:

- `fake_rotos.py --drop-chunk 20` tests resume after a lost chunk.
- `fake_rotos.py --button-every 10` sends BUTTON events.

## Incoming calls on ROTOS

When the "Call alerts on ROTOS" switch is on (the default) and the link is up,
the ride service sends CALL_STATE to ROTOS: RINGING right away and every 2 s,
ACTIVE when the call is answered, and IDLE when it ends (MISSED if nobody
answered). Button A on ROTOS sends CALL_ACTION REJECT_REPLY. The phone then
hangs up with `TelecomManager.endCall()` and texts the caller "I'm riding
right now, I'll call you back later (around HH:MM)." (ETA from Google Maps or
the engine), and sends IDLE with the REPLIED flag. Button B (DISMISS) mutes the
ring stream for that call (`call/RingerMute.kt`, allowed by notification
access) and restores it when the call ends or is answered.

Permissions (requested when the switch is on). The feature degrades when one is
missing, and the status log says what is missing:

| Permission | What it's for | Without it |
|---|---|---|
| `READ_PHONE_STATE` | detecting calls | no call alerts |
| `READ_CALL_LOG` | caller number | "UNKNOWN", no SMS |
| `READ_CONTACTS` | caller name | shows the number |
| `ANSWER_PHONE_CALLS` | hanging up | phone keeps ringing (SMS still sent) |
| `SEND_SMS` | reply text | no SMS, IDLE without REPLIED |

On a sideloaded Android 13+ phone you may first need *Settings → Apps → ROTOS →
⋮ → Allow restricted settings* (see docs/IDEAS.md section 1).

```sh
android/scripts/call_demo.sh     # adds contact "Amma", then: rejected+SMS, answered, missed
```

This uses `adb emu gsm call/accept/cancel` and `fake_rotos.py --reply-calls`.
Sent texts show up in `adb shell content query --uri content://sms/sent`.

## Notifications on ROTOS

With the "Notifications on ROTOS" switch on (the default), new notifications
from other apps (messages, alerts; not ongoing ones, group summaries, calls,
Google Maps or the system: `notify/NoteFilter.kt`) are forwarded as NOTIFY
packets, each new text once. ROTOS shows a bar at the bottom for 6 s (C opens
it) and keeps the last 8 on its notifications page (B older, C newer, hold B
clear).

## Weather page

At link start and then every 10 minutes (60 s after a failed attempt), the app
fetches current conditions plus the next 8 hours and today's sunrise/sunset
from Open-Meteo (`timezone=auto`, local time), and sends WEATHER followed by
FORECAST.

### Test hooks

`MainActivity` accepts these extras:

```
adb shell am start -n dev.rotos.app/.ui.MainActivity --ez demo true --ez sample true \
    --es link tcp --es host 10.0.2.2 --ei port 7400 --ef speedup 4 --ez autostart true
```

Other extras: `--es dest "lat,lon|place"`, `--es origin lat,lon`, `--ez calls false`, `--ez notify false`, `--ez connect true`, `--ez autoplan true`.

## Layout (`app/src/main/java/dev/rotos/app/`)

Screens and resources live in `../ui/android` (added to the Gradle source set).

| package | files |
|---|---|
| `ride` | `RideService` (foreground service, wires everything), `MapRide` / `EngineRide` (the two kinds of ride), `RerouteRules` (pure), `Dashboard`, `GuidanceMirror`, `NotifyForwarder`, `FrameSender`, `LinkOpener`, `RouteMemory` + `ResumeRule`, `RoutePlanner`, `RideNotifications` |
| `quota` | `UsageGuard` (the gate for every billed Google call), `UsagePolicy` (pure caps), `UsageLedger` (counters on disk) |
| `route` | `GoogleRoutesClient`, `GoogleRouteParser`, `RouteMatcher` (picks Google Maps' route), `RouteRepository`, `RouteLoader`, `AreaRoute` |
| `map` | `GoogleTiles` (session + style), `TileCache`, `TileWindow`, `MapPackBuilder` (PROTOCOL.md recipe, deflate for the link), `MapPreview` |
| `mirror` | `MapsNotificationListener`, `GuidanceParser`, `ArrowClassifier`, `KnownArrows`, `GuidanceBus` |
| `notify` | `PhoneNote`, `NoteFilter` (pure), `NoteReader`, `NotifyBus` |
| `call` | `CallMonitor`, `CallTracker` (pure state machine), `CallController`, `CallActions` (hang up, SMS), `RingerMute`, `ContactLookup`, `ReplyText` |
| `link` | `RotosLink`, `BleLink`, `TcpLink`, `ReconnectingLink`, `MapTransfer` (sliding window), `SeqCounter` |
| `nav` | `NativeNav` (JNI), `NavThread` (the only thread that touches the C session), `RxDecoder`, `Packets` |
| `location`, `phone`, `share`, `net`, `util` | GPS / fused location, battery + weather (Open-Meteo), Google Maps share links, HTTP, permissions + log |

Google tiles are downloaded at most once (cached under `cacheDir/gmap/`), at
most 4 at a time, and every download counts against the caps in `quota/`.
