# ROTOS — Future Ideas

Ideas discussed for ROTOS: what's built, what's planned, and what was decided against.

## Status at a glance

**Built in the digital twin**
- Day / night themes with automatic switching at sunset (phone `NIGHT` flag, palette crossfade)
- Glass (semi-transparent) top and bottom bars
- **Dashboard mode** when not navigating: mascot, big clock, date, current weather, phone status
- **Phone battery + connectivity indicator** on every screen (`PHONE_STATUS` packet)
- **Ride summary card** on arrival: distance, ride time, moving time, avg / top speed, start-end time
- **Flyover / service road indicator**: TAKE FLYOVER / STAY BELOW / EXIT FLYOVER from OSM bridge data

**Planned** (sections below)
1. Incoming calls + one-button reply
2. Map freshness
3. Performance on real hardware
4. Sunlight readability (remaining items)
5. Weather along the route + weather page
6. Mascot: choose the final design
7. Next big pieces

**Decided against** (don't re-suggest)

| Idea | Why not |
|---|---|
| Choosing destinations from ROTOS | Destinations are always set on the phone |
| Speed breaker / pothole warnings | Hyderabad has countless of them; no system can keep up |
| Ride recording + replay in the simulator | Not now; maybe later |
| Crash detection / SOS, lean angle | IMU is too tedious to deal with |
| Fuel range estimate | The instrument cluster already shows fuel and odometer |
| Service reminders | Not wanted |
| Over-the-air firmware updates | Not wanted |
| Dashboard extras beyond mascot / clock / weather / phone status | Not useful |
| Speed limits | Data in India is too sparse to trust |
| Theft alert | ROTOS has no power when the bike is off |
| Audio turn prompts | Can't hear a buzzer under a helmet |

---

## 1. Incoming calls on ROTOS + one-button "I'm riding" reply

### Goal

When the phone rings during a ride, ROTOS shows who is calling. One press of a
button rejects the call and texts the caller back. The phone stays in the pocket.

```
Phone rings
  │
  ▼
Android app detects the call, looks up the caller's name
  │  CALL_STATE packet: "incoming, AMMA"
  ▼
ROTOS shows a call popup over the map (top half only; next turn stays visible)
  │
  ├─ press A ─► ROTOS sends ACTION packet ─► phone rejects the call and sends SMS:
  │                                          "Riding right now, will call you back around 18:30"
  ├─ press B ─► dismiss the popup (phone keeps ringing; take it out if you want)
  └─ nothing ─► popup closes by itself when the call ends
```

The reply includes the ETA ROTOS already knows: "call you back around 18:30"
is more useful than "I'm driving".

### UI: Pokémon battle dialog

Buttons are named **A** and **B**, Game Boy style. The popup uses the existing
`PanelStyle::Double` (Pokémon dialog box) border.

```
┌──────────────────────────────┐
│ ☎  INCOMING CALL             │
│                              │
│    AMMA                      │
│                              │
│ ▶ A  REPLY "RIDING"          │
│   B  IGNORE                  │
└──────────────────────────────┘
```

### Android: permissions and APIs

Possible for a sideloaded personal app. Each permission is granted once.

| Feature | Android API | Permission |
|---|---|---|
| Know a call is coming in | `TelephonyCallback` (call state) | `READ_PHONE_STATE` (runtime popup) |
| Caller's number | same callback | `READ_CALL_LOG` (runtime popup); since Android 9 the number is hidden without it |
| Caller's name | contacts lookup by number | `READ_CONTACTS` (runtime popup) |
| Hang up | `TelecomManager.endCall()` | `ANSWER_PHONE_CALLS` (runtime popup). Deprecated since Android 10 but believed to still work. **Test on the actual phone.** |
| Send the SMS | `SmsManager.sendTextMessage()` | `SEND_SMS` (runtime popup); sends silently |
| Show on ROTOS | our own Wi-Fi/BLE link | none |
| Keep running in the pocket | foreground service + persistent notification | foreground-service manifest entries; turn off battery optimisation for the app |

### Catches

1. **Don't use `CallScreeningService`.** Unless the app is the default phone
   app, Android (as far as we know) only passes it calls from numbers **not**
   in contacts. Use `endCall()` instead.
2. **WhatsApp calls.** With notification access (`NotificationListenerService`),
   the app can *show* WhatsApp calls and trigger their **Decline** action. It can't
   send a WhatsApp message back unless there's already a WhatsApp message
   notification to reply to. Plan: decline, then send a normal SMS.
3. **Sideloaded apps are blocked from some settings.** On Android 13+, apps not
   from the Play Store are blocked from special access like notification access.
   One-time fix: **Settings → Apps → ROTOS → ⋮ → "Allow restricted settings"**.
   Newer Android versions may apply the same block to SMS permissions.
4. **Play Store SMS/call-log policy doesn't apply.** It only covers apps
   published on the Play Store.
5. **Phone brands add their own background restrictions** (Xiaomi, OnePlus,
   Samsung). Check the app isn't killed in the background.

**First step when this is picked up:** a throwaway Android app that only does
detect call → hang up → send SMS, tested on the actual phone, before building
anything else.

### ROTOS side

- **Protocol:** the link is currently one-way (phone → ROTOS). Add a return
  channel: ROTOS → phone `ACTION` packets, acknowledged with the existing `ACK` type
  and `SEQ` field. New packet types:
  - `CALL_STATE` (phone → ROTOS): `{state: incoming|active|ended, caller name, number}`
  - `ACTION` (ROTOS → phone): `{action: reject_and_reply | dismiss, call id}`
- **Hardware buttons:** XIAO ESP32-C3 has 11 GPIO; the ILI9341 uses 4–5
  (SCK, MOSI, CS, DC, RST), leaving room for 3–4 buttons. Use 2–3 big tactile
  buttons you can press with gloves on, sealed against rain, with debouncing in firmware.
- **Safety rules:**
  - one press, no menus, nothing that needs reading
  - the popup covers only the top half; the next turn stays visible
  - optional setting: auto-reply to every caller while moving above 10 km/h, no button needed

### Prototype in the digital twin first

- `rotos_screen`: keyboard **A** / **B** act as the hardware buttons,
  sent back to ROTOS over the TCP link (the screen twin grows a "button" side
  channel, like GPIO pins on a real board).
- `phone/ride_sim.py`: insert a fake `CALL_STATE` (incoming call) packet
  partway through the ride.
- `rotos_sim`: logs the `ACTION` packet ROTOS sends back, standing in for
  the phone hanging up and texting.

---

## 2. Map freshness (OSM lags Google)

Measured on the test route corridor (Gachibowli → Durgam Cheruvu, Oct 2026):
3,021 road segments, 79% of main roads edited in 2025–26, newest edit 27 May
2026. OSM is actively maintained but trails Google by weeks to months on new
construction. The route always comes from Google, so OSM staleness only affects
the background picture.

- **Tile cache expiry:** `phone/cache/` currently never expires. Re-download tiles
  older than ~7 days, using the server's change markers (`ETag` / `If-None-Match`)
  to skip unchanged tiles. Download only route corridors, never whole areas
  (OSM blocks clients that download heavily).
- **Stale-map detection:** check whether Google's route runs over road
  pixels in the quantized map. Where it crosses buildings or empty land for more
  than ~30 m, paint a road under the route and log the spot so it can be fixed on
  openstreetmap.org.
- **Optional:** fetch road data from the Overpass API instead of map images
  (minute-level freshness, crisper roads, but we'd draw the roads ourselves).

---

## 3. Performance on real hardware

- The rotating map changes almost every band, so each frame costs ~141 KiB of
  SPI ≈ 29 ms at 40 MHz, which means ~15–20 fps on the real ESP32. Skip
  redraws when the camera moved < 1 px / rotated < ~1°, and redraw only HUD
  bands when only the numbers change.
- The map pack (~794 KiB for 6.8 km) won't fit in ESP32-C3 RAM, but fits in its
  4 MB flash. Store it in a flash partition and read it in place
  (`MapPack` already parses in place).

---

## 4. Sunlight readability

Day/night auto-switching is implemented (phone sends `NAV_NIGHT` from sun
elevation, ROTOS crossfades palettes). Still open:

- **Ambient light sensor** (already an optional BOM item): feed the same night
  flag for tunnels and heavy shade, and drive backlight brightness. Needs
  hysteresis, since the 3 s hold in `Rotos::updateTheme` is tuned for sun-based flips.
- **`--sun` preview in the screen twin:** add a grey haze and cut contrast to
  roughly simulate glare, to compare themes before hardware arrives.
- **First hardware test:** take the TFT outside at noon and compare day vs night themes.
- **Hardware options if glare wins:** anti-glare matte film, visor in the
  enclosure, tilt away from the sky; a reflective colour memory-in-pixel LCD
  (Game Boy / Garmin watch approach) as a future revision.

---

## 5. Weather: weather page + weather along the route

### Goal

Two things:

1. **Weather page:** current conditions and the next few hours, on a screen
   you can switch to.
2. **Weather on the way:** what you'll ride into, e.g. "RAIN IN 8 KM", shown
   on the map without leaving navigation.

The key point for (2): the forecast must match **where you'll be and when you'll
get there**, not the weather everywhere right now. For each point along the route, take
its ETA (the phone already knows distance and speed) and look up the forecast
for that hour at that place.

```
route ──► sample every ~3–5 km ──► ETA at each sample ──► hourly forecast at (place, ETA)
                                                              │
                                     WEATHER packet ◄─────────┘
                                          │
                                          ▼
                     ROTOS: rain zones on the route + "RAIN IN 8 KM" chip + weather page
```

### Phone side

- **Data source:** [Open-Meteo](https://open-meteo.com/). Free, no API key,
  hourly forecasts with rain probability, rain amount, wind and temperature, and
  one request can cover many coordinates. Alternatives: Google Weather API
  (Maps Platform key), OpenWeatherMap (key).
- **When to fetch:** at route start, every ~20–30 min, and on reroute. Not
  continuously: weather data only changes hourly.
- **Honest limit:** forecasts are on a grid of a few km and hourly. Treat
  "rain in 8 km" as a strong hint, not radar. In India, monsoon storms build
  faster than hourly models update.

### Protocol

New packet type `WEATHER` (phone → ROTOS), small enough for one packet:

- `now`: temperature, condition code, wind speed, rain probability
- `next_hours[6]`: hourly condition + temperature + rain probability (for the page)
- `route_samples[≤16]`: `{along_m, eta, condition, rain_prob, rain_mm, temp}`

ROTOS caches the last `WEATHER` packet like the route, so it keeps working
through link dropouts.

### ROTOS UI

- **Rain zones on the map:** route segments where rain is likely are drawn
  with a dithered pattern (route colour + `C_WATER`). Rain ahead is visible at a
  glance, and no new palette entry is needed.
- **Warning chip:** "RAIN IN 8 KM" / "STORM AHEAD" in the chip row under the
  turn panel. Shown only when it matters (e.g. rain probability > 60% within
  the next 20 km).
- **Weather page:** full screen, Pokémon-style 16×16 pixel weather icons (sun,
  cloud, rain, storm, fog, wind) scaled 3×; current temperature in big digits; an
  hourly strip; and a route strip showing weather along the remaining ride.

  ```
  ┌──────────────────────────────┐
  │  ☁☂   27°   RAIN 70%         │
  │  WIND 14 KM/H                │
  │                              │
  │  18  19  20  21  22  23      │
  │  ☂   ☂   ☁   ☁   ☾   ☾       │
  │                              │
  │  ON THE WAY                  │
  │  YOU ─────▓▓▓▓───── DEST     │
  │        RAIN 4–9 KM           │
  └──────────────────────────────┘
  ```

- **Page-specific palette:** the weather page can swap to its own 16 colours
  (sky blues, sun yellow), NES style. The map palette stays untouched, and the
  palette crossfade from day/night switching gives a smooth page change for free.

### Switching pages

- Needs the hardware buttons from section 1: e.g. **B** cycles
  Map → Weather → (later) Trip stats.
- Safety: while moving, any page other than the map returns to the map
  automatically after ~8 s. Rain warnings appear on the map, so the page is
  never *needed* while riding.

### Prototype in the digital twin first

- `phone/build.py`: query Open-Meteo for the route samples at their ETAs and
  write `WEATHER` packets into `ride.rfeed`. Also add a `--mock-weather` option
  that injects a rain zone (e.g. km 3–4) for repeatable demos.
- Screen twin: keyboard **B** cycles pages, as with the call buttons.

---

## 6. Mascot: choose the final design

The dashboard and ride summary card already show a mascot. The current one is a
**placeholder** (an original little robot, 16×16, 3 moods: idle, blink,
happy) in `core/icons.cpp`. To swap it, replace the three sprite arrays; nothing
else changes. Open questions:

- Which character: an original design of our own, or something inspired by
  Rotom (the name's origin). For a private device anything goes, but an original
  design could go on the enclosure and boot screen too.
- More moods: sleeping (parked, late night), rain (when the weather shows rain),
  excited (just before arrival).
- Should it also appear small during navigation, or stay dashboard-only?

---

## 7. Next big pieces

- Send the map pack over Wi-Fi as chunked `ROUTE_UPDATE` packets (currently a file).
- Android app: Google Routes API → same pipeline as `phone/`.
- Real hardware bring-up: swap `host/tcp_spi_bus.cpp` for the ESP32 SPI + DMA bus.
