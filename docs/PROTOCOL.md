# Phone ↔ ROTOS link protocol (version 2)

Source of truth: `core/include/rn/rn_protocol.h` (structs + codecs) and
`core/include/rn/rn_mappack.h` (map pack format). This page explains
how the pieces are used over a real link.

## Transports

The protocol is a byte stream; it does not care how bytes travel.

| Transport | Used by | Details |
|---|---|---|
| **BLE GATT** | real hardware | ROTOS advertises as `ROTOS`. Service `6e400001-b5a3-f393-e0a9-e50e24dcca9e` (Nordic-UART layout). Phone → ROTOS: write (without response) to characteristic `6e400002-b5a3-f393-e0a9-e50e24dcca9e`. ROTOS → phone: notifications on `6e400003-b5a3-f393-e0a9-e50e24dcca9e`. The phone requests ATT MTU 517 and data length extension, so a full frame (≤ 258 bytes) fits in one write. |
| **TCP** | simulator | `rotos_sim --listen 7400` accepts one phone connection. The Android emulator reaches it at `10.0.2.2:7400` (or `127.0.0.1:7400` with host networking). |
| **.rfeed file** | simulator replay | Timestamped capture: records of `u32 deliver_ms, u16 len, bytes[len]`. One-way (phone → ROTOS), no ACKs needed. |

## Frame

```
| A5 5A | VERSION=2 | TYPE | SEQ u16 | LEN u16 | PAYLOAD[LEN ≤ 248] | CRC16 u16 |
```

Little-endian. CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over
`VERSION … PAYLOAD`. A frame can be split across any number of writes or
segments; the parser resyncs on garbage and counts CRC errors and lost
sequence numbers.

## Packets

| Type | Name | Direction | When | Payload |
|---|---|---|---|---|
| 0x01 | NAV_STATE | phone → ROTOS | 1 Hz while riding | 38 B: t_ms, route_id, lat/lon ×1e7, speed cm/s, heading 0.01°, along m, remaining m, remaining s (0 in Google Maps mode: ROTOS works out time left from the ETA), ETA (local s of day; 0 = unknown), flags (0x04 = Google Maps / the phone is rerouting), accuracy m |
| 0x02 | PHONE_STATUS | phone → ROTOS | every 5 s | 14 B: t_ms, local time of day, weekday, day, month, battery %, flags (0x01 charging, 0x02 night, 0x04 Google usage near its cap: ROTOS shows G!), signal bars 0-4 |
| 0x03 | WEATHER | phone → ROTOS | every 10 min | 10 B: t_ms, temp ×10 °C, WMO code, rain %, wind km/h, is_day |
| 0x04 | SETTINGS | phone → ROTOS | on change | 4 B: theme mode (0 auto, 1 day, 2 night), north-up, glass %, brightness % (0 = auto) |
| 0x05 | FORECAST | phone → ROTOS | every 10 min (with WEATHER) | 10 + 5n B: t_ms, start hour (local), n ≤ 8, sunrise and sunset (local minutes of day), then n × (temp ×10 °C, WMO code, rain %, wind km/h) |
| 0x06 | CALL_STATE | phone → ROTOS | on every call change, and every 2 s while ringing | 32 B: t_ms, call_id, state (0 idle/ended, 1 ringing, 2 answered), flags (1 replied, 2 missed), name[24] (contact name or number, upper-case ASCII) |
| 0x07 | PREP_STATUS | phone → ROTOS | ~1 Hz while the phone prepares a route (after connecting, before MAP_BEGIN) | 10 B: t_ms, stage (0 idle, 1 route, 2 flyovers, 3 map, 4 ready, 5 failed), pct 0-100 for the whole job, done u16, total u16 (map blocks; 0/0 = no count) |
| 0x08 | GUIDANCE | phone → ROTOS | on every Google Maps update and every 2 s while it navigates ("Google Maps mode") | 48 B: t_ms, maneuver (RnManeuverType), flags (1 active, 2 arrived), to-turn m u16, remaining m u32, ETA local s of day u32, road[32] (upper-case ASCII); unknown numbers all-ones. Flags 0 = guidance ended |
| 0x09 | NOTIFY | phone → ROTOS | a new phone notification (messages, app alerts; not ongoing, calls or Google Maps) | 168 B: t_ms, id u16 (same id = updated notification), kind (0 message, 1 other), reserved, app[16], title[24], text[120] (upper-case ASCII). ROTOS keeps the last 8: a bar at the bottom for 6 s (C opens it), and a notifications page in the A cycle (B older, C newer, hold B clear) |
| 0x0A | ROUTE | phone → ROTOS | ride start, every reroute, after a reconnect | 17 + 12n B: line id, pack route id, total points (≤ 1000), first, n (≤ 18), total m, n × (x, y in the pack's pixels, along m). The line ROTOS draws instead of the pack's own route; a reroute sends only this (~14 KB at most, well under a second). ROTOS answers `ACK(ROUTE, OK, value = line id)` when every point arrived; the phone resends otherwise |
| 0x10 | MAP_BEGIN | phone → ROTOS | route start / reroute | 16 B: pack length, pack CRC-32, route_id, wire length (0 = raw; else the chunks carry raw deflate, 4 KB window). 12-byte form = raw |
| 0x11 | MAP_CHUNK | phone → ROTOS | | 6 + n B: offset u32 (wire bytes), n u16, data (n ≤ 240) |
| 0x12 | MAP_END | phone → ROTOS | | 8 B: total length, CRC-32 |
| 0x20 | PING | both | every 2 s when idle | empty |
| 0x21 | ACK | ROTOS → phone | map transfer | 8 B: acked type, status, acked seq, value |
| 0x22 | BUTTON | ROTOS → phone | button press | 2 B: button 0-2, event (1 short, 2 long) |
| 0x23 | CALL_ACTION | ROTOS → phone | call popup button | 4 B: call_id, action (1 reject + reply SMS, 2 dismiss), reserved |

NAV_STATE flags: `0x01 GPS_OK, 0x02 OFF_ROUTE, 0x04 REROUTING, 0x08 NIGHT, 0x10 ARRIVED, 0x20 FREE` (position only: Google Maps navigates), `0x40 LINE` (with FREE: draw the pack's route, `along` is valid), `0x80 HEADING` (with FREE: the heading is a real GPS course).

GUIDANCE ("Google Maps mode"): the rider navigates in Google Maps; the app
reads its turn-by-turn notification (`MapsNotificationListener`, needs
notification access) and forwards each turn. ROTOS draws a big arrow, the
distance, the road, arrival time and distance left, without a map pack, on
its dashboard page, and drops back to the dashboard 8 s after the last
GUIDANCE. A ROTOS ride of its own takes precedence.

PREP_STATUS: the app connects to ROTOS first and plans afterwards, so ROTOS
leaves "waiting for phone" at once and shows the progress in its footer panel
("PHONE: BUILDING MAP 12/37"). The panel hides on stage 0, when the map
transfer starts, or 10 s after the last PREP_STATUS. First planning in a new
area downloads Google map tiles and takes a little longer.

Link drop-outs: the app reconnects by itself (backoff 1 to 4 s between 15 s scans, 30 s and low-power scans once ROTOS has been away ~10 min) and carries on
sending NAV_STATE. ROTOS keeps the map pack in flash, so a reconnect, or a
ROTOS power cycle, resumes the ride without a new map transfer. A transfer that
was interrupted is restarted after the reconnect.

`route_id` ties navigation packets to the map pack they belong to: ROTOS
ignores NAV_STATE whose `route_id` differs from the stored pack (it shows
"waiting for map" instead of drawing a wrong route).

## Calls

```
phone                                         ROTOS
  CALL_STATE(id, RINGING, "AMMA")   ──────►   bar at the bottom: A = REPLY "RIDING", B = IGNORE
  (repeated every 2 s while ringing)
                                    ◄──────   CALL_ACTION(id, REJECT_REPLY)   button A
  end the call, send the SMS
  CALL_STATE(id, IDLE, flags=REPLIED) ─────►  "REPLY SENT", popup closes after 2 s
```

- B sends `CALL_ACTION(id, DISMISS)`: ROTOS hides the popup and the phone
  keeps ringing (take it out of your pocket if you want to answer).
- Answered on the phone or ended by the caller: `CALL_STATE(id, ACTIVE or IDLE)`
  closes the popup.
- With no CALL_STATE for 10 s, ROTOS assumes the call is over (link lost).
- The reply text is built by the phone and includes the ETA when navigating:
  "I'm riding right now, I'll call you back later (around 18:30)."

## Map transfer

A map pack is ~150-300 KB for a city route. It is sent once per route over the
same link, with a sliding window so BLE never overflows ROTOS's buffers:

```
phone                                   ROTOS
  MAP_BEGIN(len, crc, route_id)  ──────►  erase flash for len bytes
                                 ◄──────  ACK(MAP_BEGIN, OK|TOO_BIG|STORAGE, value=0)
  MAP_CHUNK(offset=0)            ──────►  write if offset == received
  MAP_CHUNK(offset=240)          ──────►
  ...  (at most 40 chunks, 10 KB, beyond the last acknowledged byte count)
                                 ◄──────  ACK(MAP_CHUNK, OK, value=received)  every 8 chunks
  ...
  MAP_END(len, crc)              ──────►  verify CRC-32 over the stored bytes
                                 ◄──────  ACK(MAP_END, OK|BAD_CRC, value=len)
```

- A chunk whose offset is not exactly the number of bytes received so far
  is ignored, and ROTOS answers `ACK(MAP_CHUNK, BAD_STATE, value=received)`;
  the phone resumes from `value`.
- No ACK progress for 3 s: the phone resends from the last acknowledged value.
- `MAP_BEGIN` for the exact pack ROTOS already stores (same length and CRC-32)
  is answered `ACK(MAP_BEGIN, HAVE=5, value=len)`: nothing is sent, the stored
  pack stays active.
- BLE: ATT MTU 517 and data length extension, so frames go out in ~514-byte
  writes; the phone batches a 40-chunk window into one write call.
- Compression: the phone deflates the pack (raw deflate, 4 KB window,
  ~40 % smaller); ROTOS inflates each chunk as it arrives
  (`core/src/base/rn_inflate.c`) and writes the output to flash. Offsets and
  ACK values count wire bytes; MAP_END's length and CRC are the pack's.
- Two flash slots: the new pack goes into the slot the current one does not
  use, so ROTOS keeps drawing the current map during the transfer and
  switches on `ACK(MAP_END, OK)`; a failed transfer leaves it untouched. The
  free slot is erased a sector per frame in the background, so MAP_BEGIN
  is answered at once (an unprepared slot used to cost ~4.5 ms per KB).
- After `ACK(MAP_END, OK)` the new pack becomes active (and survives a
  power cycle). NAV_STATE packets with the new `route_id` follow.
- In `.rfeed` replays the chunks simply arrive in order without waiting.

## Phone side map pack recipe

The Android app builds packs from Google tiles; the simulator's virtual phone
does the same with OpenStreetMap tiles (offline testing, `mapgen_source` 0), through the navigation library (`rn_ffi.h`):

1. Route from Google Routes API (`travelMode: TWO_WHEELER`): add each
   step's polyline (`rn_ffi_route_add_polyline` returns the step's first point
   index) and its maneuver (`rn_ffi_route_add_maneuver_google`), then
   `rn_ffi_route_finish(duration_s, route_id)`.
2. Optional flyover hints from road names: `rn_ffi_flyover_by_name()`
   (the virtual phone can also add bridge geometry with `rn_ffi_bridges_add`
   and `rn_ffi_flyover_annotate()`).
3. `rn_ffi_corridor_plan(16, 260)` → origin (world px at zoom 16), tiles_w × tiles_h
   cells of 32 px, keep mask.
4. Blocks of 8 × 8 cells (256 px at zoom 16). For every block containing a
   kept cell: the input is the zoom-17 image of the block grown by a 16 px
   (zoom 16) halo on every side, i.e. a 576 × 576 RGB window starting at
   zoom-17 world pixel `((origin_x + bx·256 − 16)·2, (origin_y + by·256 − 16)·2)`,
   stitched from Google Map Tiles API 2D tiles in ROTOS's style
   (`GoogleTiles.kt`; cached on disk, missing tiles left black), with
   `rn_ffi_mapgen_source(1)` so narrow lanes are kept.
   `rn_ffi_mapgen_block(rgb, 576, 576, out)` → 288 × 288 classes; the
   block's interior starts at (16, 16). Each kept cell's 32 × 32 classes go
   to `rn_ffi_pack_add_tile(cx, cy, classes)`.
5. `rn_ffi_pack_finish()`, then MAP_BEGIN / MAP_CHUNK… / MAP_END frames from
   `rn_ffi_frame_map_*`.
