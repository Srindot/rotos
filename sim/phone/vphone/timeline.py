"""The phone's side of a session as a time-ordered list of frames:

  t = 0           phone connects: PHONE_STATUS every 5 s, WEATHER every 60 s
  t = 0.3 s       PREP_STATUS ~1 Hz: route, flyovers, map blocks n/N, ready
                  (the app connects first, then plans, like the Android app)
  t = 7 s         map pack transfer (MAP_BEGIN, MAP_CHUNK x N, MAP_END),
                  paced like BLE
  ride start      NAVIGATION_STATE at 1 Hz from the simulated ride
  after arrival   dashboard time, statuses continue

Frames are built in time order by the shared C library, with one sequence
counter for the link, exactly as the Android app would send them.
"""
import math
import random
from datetime import timedelta

import rotos_nav as rn

STATUS_EVERY_MS = 5000
WEATHER_EVERY_MS = 60000
FORECAST_EVERY_MS = 600000
CALL_REPEAT_MS = 2000

# Scripted calls during the ride (seconds after the ride starts):
#   AMMA rings, the rider presses A, the phone rejects and texts back
#   RAVI OFFICE rings out (ignored / missed)
CALLS = [
    {"at_s": 90, "id": 1, "name": "Amma", "ring_s": 6, "replied": True},
    {"at_s": 300, "id": 2, "name": "Ravi Office", "ring_s": 12, "replied": False},
]
# Scripted phone notifications (seconds after the ride starts).
NOTES = [
    {"at_s": 40, "id": 11, "app": "WhatsApp", "title": "Amma", "text": "Where are you? Dinner is ready, come home soon."},
    {"at_s": 55, "id": 12, "app": "Messages", "title": "HDFC Bank", "text": "Rs 450.00 spent on your card at HP PETROL PUMP. Avl bal Rs 12,340."},
]
MAP_START_MS = 7000
PREP_MAP_STEPS = 4                              # map-block progress updates


def prep_specs(nav, blocks):
    """PREP_STATUS frames before the map transfer: [(t_ms, order, type, builder)]."""
    steps = [(300, rn.PREP_ROUTE, 5, 0, 0), (1300, rn.PREP_BRIDGES, 15, 0, 0)]
    for k in range(PREP_MAP_STEPS + 1):
        done = blocks * k // PREP_MAP_STEPS
        steps.append((2300 + k * 1000, rn.PREP_MAP, 20 + 75 * k // PREP_MAP_STEPS, done, blocks))
    steps.append((MAP_START_MS - 300, rn.PREP_READY, 100, 0, 0))
    return [(t, 1, rn.PREP_STATUS, lambda seq, t=t, s=s: nav.frame_prep_status(seq, t, s[0], s[1], s[2], s[3]))
            for (t, *s) in steps]


def simulate_ride(nav, seed=7, cruise_mps=12.0, sigma_m=3.0):
    """Synthetic rider: [(t_s, lat, lon, speed, heading, accuracy)] at 1 Hz,
    ending 3 s after arrival."""
    nav.sim_start(seed, cruise_mps)
    fixes, t, riding, tail = [], 0.0, True, 0
    for _ in range(4 * 3600):                       # 4 h cap
        lat, lon, spd, hdg, acc = nav.sim_fix(sigma_m)
        fixes.append((t, lat, lon, spd, hdg, acc))
        if not riding:
            tail += 1
            if tail > 3:
                break
        for _ in range(20):
            riding = nav.sim_step(0.05) and riding
        t += 1.0
    return fixes


def map_transfer_times(pack_len, chunk, rate_bps, start_ms=MAP_START_MS):
    """Delivery times of MAP_BEGIN, each chunk, MAP_END at rate_bps."""
    n = math.ceil(pack_len / chunk)
    per = chunk / rate_bps * 1000.0
    chunks = [start_ms + 50 + int(i * per) for i in range(n)]
    return start_ms, chunks, (chunks[-1] if chunks else start_ms) + 50


class Clock:
    """Link time (ms since the phone connected) <-> wall clock."""

    def __init__(self, ride_local_start, ride_t0_ms, utc_offset_s):
        self.start = ride_local_start            # aware datetime at ride start
        self.t0 = ride_t0_ms
        self.off = utc_offset_s

    def local(self, t_ms):
        return self.start + timedelta(milliseconds=t_ms - self.t0)

    def unix(self, t_ms):
        return self.local(t_ms).timestamp()


def call_specs(nav, clock):
    """CALL_STATE frames for the scripted calls (replay: the phone's half only)."""
    specs = []
    for c in CALLS:
        t0 = clock.t0 + c["at_s"] * 1000
        for t in range(t0, t0 + c["ring_s"] * 1000, CALL_REPEAT_MS):
            specs.append((t, 4, rn.CALL_STATE, lambda seq, t=t, c=c: nav.frame_call_state(
                seq, t, c["id"], rn.CALL_RINGING, 0, c["name"])))
        end = t0 + c["ring_s"] * 1000 + (1500 if c["replied"] else 0)
        flags = rn.CALL_F_REPLIED if c["replied"] else rn.CALL_F_MISSED
        specs.append((end, 4, rn.CALL_STATE, lambda seq, t=end, c=c, f=flags: nav.frame_call_state(
            seq, t, c["id"], rn.CALL_IDLE, f, c["name"])))
    return specs


def notify_specs(nav, clock):
    """NOTIFY frames for the scripted notifications."""
    return [(clock.t0 + n["at_s"] * 1000, 4, rn.NOTIFY, lambda seq, t=clock.t0 + n["at_s"] * 1000, n=n:
             nav.frame_notify(seq, t, n["id"], rn.NOTIFY_MESSAGE, n["app"], n["title"], n["text"]))
            for n in NOTES]


def press_times(clock, feed_start_ms):
    """Device times (ms) at which rotos_sim should press A for replied calls."""
    return [feed_start_ms + clock.t0 + c["at_s"] * 1000 + (c["ring_s"] - 2) * 1000
            for c in CALLS if c["replied"]]


def build(nav, pack_len, fixes, clock, wx, end_ms, rate_bps=20000, battery0=78, seed=11, fc=None, google_cap=False):
    """-> [(t_ms, type, frame bytes)] in send order. google_cap: the phone reports its Google limit."""
    rnd = random.Random(seed)
    specs = []                                  # (t_ms, order, type, builder(seq))
    lat0, lon0 = fixes[0][1], fixes[0][2]
    bars = [4]

    def status(t):
        if (t // STATUS_EVERY_MS) % 3 == 0:     # mobile signal wanders a little
            bars[0] = max(1, min(4, bars[0] + rnd.choice((-1, 0, 0, 1))))
        now = clock.local(t)
        night = nav.sun_elevation(lat0, lon0, clock.unix(t)) < -0.833
        flags = (rn.STATUS_NIGHT if night else 0) | (rn.STATUS_GOOGLE_CAP if google_cap else 0)
        secs = now.hour * 3600 + now.minute * 60 + now.second
        b = max(5, battery0 - t // 90000)
        return lambda seq: nav.frame_phone_status(seq, t, secs, now.weekday(), now.day, now.month, b, flags, bars[0])

    for t in range(0, end_ms + 1, STATUS_EVERY_MS):
        specs.append((t, 0, rn.PHONE_STATUS, status(t)))
    for t in range(1, end_ms + 1, WEATHER_EVERY_MS):
        specs.append((t, 1, rn.WEATHER, lambda seq, t=t: nav.frame_weather(
            seq, t, wx["temp_c"], wx["code"], wx["rain_prob"], wx["wind_kmh"], wx["is_day"])))
    for t in range(2, end_ms + 1, FORECAST_EVERY_MS):
        if fc:
            specs.append((t, 1, rn.FORECAST, lambda seq, t=t: nav.frame_forecast(
                seq, t, fc["start_hour"], fc["sunrise_min"], fc["sunset_min"], fc["hours"])))
    specs.extend(call_specs(nav, clock))
    specs.extend(notify_specs(nav, clock))
    specs.extend(prep_specs(nav, max(1, pack_len // 7400)))   # ~7.4 KB per map block
    begin, chunks, end = map_transfer_times(pack_len, nav.map_chunk_size(), rate_bps)
    specs.append((begin, 2, rn.MAP_BEGIN, nav.frame_map_begin))
    for i, t in enumerate(chunks):
        specs.append((t, 2, rn.MAP_CHUNK, lambda seq, off=i * nav.map_chunk_size(): nav.frame_map_chunk(seq, off)))
    specs.append((end, 2, rn.MAP_END, nav.frame_map_end))
    nav.engine_reset()
    for (ts, lat, lon, spd, hdg, acc) in fixes:
        t = clock.t0 + int(ts * 1000)
        specs.append((t, 3, rn.NAV_STATE, lambda seq, t=t, f=(lat, lon, spd, hdg, acc): nav.engine_fix(
            seq, t, clock.unix(t), clock.off, f[0], f[1], f[2], f[3], f[4])))
    specs.sort(key=lambda s: (s[0], s[1]))
    return [(t, ptype, build_fn(seq & 0xFFFF)) for seq, (t, _, ptype, build_fn) in enumerate(specs)]
