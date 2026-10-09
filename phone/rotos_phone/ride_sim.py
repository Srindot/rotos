"""Simulated ride: a rider following the route, a noisy phone GPS, and the
phone's localization (map matching) producing NAVIGATION_STATE packets."""
import math
import random
from datetime import timedelta, timezone

from . import geo, protocol, sun
from .google_route import MANEUVER as M

# Comfortable speed through each maneuver, m/s
TURN_SPEED = {
    M["TURN_SLIGHT_LEFT"]: 7, M["TURN_SLIGHT_RIGHT"]: 7,
    M["TURN_LEFT"]: 4.5, M["TURN_RIGHT"]: 4.5,
    M["TURN_SHARP_LEFT"]: 3.5, M["TURN_SHARP_RIGHT"]: 3.5,
    M["UTURN_LEFT"]: 2.5, M["UTURN_RIGHT"]: 2.5,
    M["ROUNDABOUT_LEFT"]: 4.5, M["ROUNDABOUT_RIGHT"]: 4.5,
    M["FORK_LEFT"]: 8, M["FORK_RIGHT"]: 8, M["RAMP_LEFT"]: 7, M["RAMP_RIGHT"]: 7,
    M["MERGE"]: 8, M["ARRIVE"]: 0,
}
ACCEL, DECEL = 1.2, 1.8


class Polyline:
    def __init__(self, route):
        self.p, self.s = route.points, route.along

    def at(self, s):
        """(lat, lon, bearing) at distance s along the route."""
        s = min(max(s, 0), self.s[-1])
        lo, hi = 0, len(self.s) - 1
        while hi - lo > 1:
            mid = (lo + hi) // 2
            lo, hi = (mid, hi) if self.s[mid] <= s else (lo, mid)
        seg = max(self.s[hi] - self.s[lo], 1e-6)
        t = (s - self.s[lo]) / seg
        a, b = self.p[lo], self.p[hi]
        return a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, geo.bearing(a, b)

    def match(self, lat, lon, s_lo, s_hi):
        """Project a GPS fix onto the route, searching only [s_lo, s_hi]."""
        best = (float("inf"), s_lo)
        k = math.cos(math.radians(lat)) * 111320.0
        for i in range(len(self.p) - 1):
            if self.s[i + 1] < s_lo or self.s[i] > s_hi:
                continue
            (la, lo_), (lb, lob) = self.p[i], self.p[i + 1]
            ax, ay = (lo_ - lon) * k, (la - lat) * 110540.0
            bx, by = (lob - lon) * k, (lb - lat) * 110540.0
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, -(ax * dx + ay * dy) / max(dx * dx + dy * dy, 1e-9)))
            d = math.hypot(ax + dx * t, ay + dy * t)
            if d < best[0]:
                best = (d, self.s[i] + (self.s[i + 1] - self.s[i]) * t)
        return best  # (cross-track metres, along metres)


def simulate(route, start, t0_s=0.0, seed=7, gps_sigma=3.0, cruise=12.0):
    """start: timezone-aware local datetime when the ride begins; t0_s: link
    time at which it begins. Returns ([(t_ms, type, payload)], ride seconds)."""
    start_clock_s = start.hour * 3600 + start.minute * 60 + start.second
    rnd = random.Random(seed)
    line = Polyline(route)
    total = route.total_m

    stops = [(m.along_m, TURN_SPEED[m.type], 0.0) for m in route.maneuvers if m.type in TURN_SPEED]
    stops.append((total * 0.33, 0.0, 12.0))  # a red light
    stops.sort()

    def cruise_at(s):
        name = next((m.road for m in reversed(route.maneuvers) if m.along_m <= s), "")
        return 15.0 if "FLYOVER" in name.upper() else cruise

    events = []
    t, s, v, dwell, next_fix = 0.0, 0.0, 0.0, 0.0, 0.0
    matched, dt = 0.0, 0.05
    while True:
        if t >= next_fix:
            next_fix += 1.0
            lat, lon, brg = line.at(s)
            # phone GPS: noisy position, heading only meaningful when moving
            lat += rnd.gauss(0, gps_sigma) / 110540.0
            lon += rnd.gauss(0, gps_sigma) / (110540.0 * math.cos(math.radians(lat)))
            hdg = (brg + rnd.gauss(0, 4)) % 360
            # phone localization: snap to route near the previous match
            xtrack, matched = line.match(lat, lon, matched - 20, matched + 60)
            flags = protocol.FLAG_GPS_OK | (protocol.FLAG_OFF_ROUTE if xtrack > 40 else 0)
            if sun.is_night(lat, lon, (start + timedelta(seconds=t)).astimezone(timezone.utc)):
                flags |= protocol.FLAG_NIGHT
            remaining = max(total - matched, 0.0)
            # ETA: scale the route's own duration estimate by what is left
            remaining_s = route.duration_s * remaining / max(total, 1.0)
            spd = max(0.0, v + rnd.gauss(0, 0.2)) if v > 0.1 else 0.0
            t_ms = int((t0_s + t) * 1000)
            events.append((t_ms, protocol.NAVIGATION_STATE,
                           protocol.nav_state(t_ms, lat, lon, spd, hdg, matched, remaining, remaining_s,
                                              start_clock_s + t + remaining_s, flags, gps_sigma * 1.5)))
            if s >= total and t > 3:
                break

        if dwell > 0:
            dwell -= dt
        else:
            lim = cruise_at(s)
            for a, vt, _ in stops:
                if a > s:
                    lim = min(lim, math.sqrt(vt * vt + 2 * DECEL * (a - s)))
            v = min(v + ACCEL * dt, lim)
            s += v * dt
            hit = next((st for st in stops if st[1] == 0 and 0 <= st[0] - s < 0.8), None)
            if hit:
                s, v, dwell = hit[0], 0.0, hit[2]
                stops.remove(hit)
            if s >= total:
                s, v = total, 0.0
        t += dt

    return events, t


def phone_timeline(start, t0_s, end_s, lat, lon, wx, seed=11):
    """PHONE_STATUS every 5 s and WEATHER every 60 s from link time 0 to end_s.
    start: local datetime at link time t0_s (ride start)."""
    rnd = random.Random(seed)
    events, bars = [], 4
    for k in range(int(end_s // 5) + 1):
        t = k * 5.0
        now = start + timedelta(seconds=t - t0_s)
        if k % 3 == 0:  # mobile signal wanders a little
            bars = max(1, min(4, bars + rnd.choice((-1, 0, 0, 1))))
        battery = max(5, 78 - int(t / 90))           # ~1% every 1.5 min while navigating
        night = sun.is_night(lat, lon, now.astimezone(timezone.utc))
        events.append((int(t * 1000), protocol.PHONE_STATUS,
                       protocol.phone_status(int(t * 1000), now, battery, False, bars, night)))
        if k % 12 == 0 and wx:
            events.append((int(t * 1000) + 1, protocol.WEATHER,
                           protocol.weather(int(t * 1000), wx["temp_c"], wx["code"], wx["rain_prob"],
                                            wx["wind_kmh"], wx["is_day"])))
    return events
