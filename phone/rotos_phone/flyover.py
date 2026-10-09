"""Flyover vs service road hints.

Google says "slight right" or nothing at all where a flyover splits from the
road below, which is exactly where riders get it wrong. Using OSM bridge
geometry (Overpass API) we tag maneuvers with:

  TAKE   the route climbs onto a flyover here
  EXIT   the route comes down off a flyover here
  STAY   a flyover starts here in our direction, but the route stays below

and add a STRAIGHT maneuver where Google gives no instruction but a STAY or
TAKE decision exists. Falls back to road names ("... Flyover") when offline.
"""
import hashlib
import json
import math
import os
import urllib.parse
import urllib.request

from .google_route import MANEUVER as M, Maneuver
from .ride_sim import Polyline

NONE, TAKE, STAY, EXIT = 0, 1, 2, 3          # must match core/nav.h FlyoverHint
NAMES = {TAKE: "TAKE FLYOVER", STAY: "STAY BELOW", EXIT: "EXIT FLYOVER"}

MIRRORS = ["https://overpass.private.coffee/api/interpreter",
           "https://overpass-api.de/api/interpreter",
           "https://overpass.kumi.systems/api/interpreter"]
ROADS = "motorway|trunk|primary|secondary|tertiary|unclassified|residential"
MIN_BRIDGE_M = 120        # shorter "bridges" are culverts over drains, not flyovers
DECISION = (M["DEPART"], M["NAME_CHANGE"], M["ARRIVE"])


def fetch_bridges(route, cache_dir):
    lats = [p[0] for p in route.points]
    lons = [p[1] for p in route.points]
    bbox = f"{min(lats) - .005},{min(lons) - .005},{max(lats) + .005},{max(lons) + .005}"
    q = f'[out:json][timeout:90];way["highway"~"^({ROADS})(_link)?$"]["bridge"="yes"]({bbox});out geom;'
    path = os.path.join(cache_dir, "overpass_" + hashlib.sha1(q.encode()).hexdigest()[:12] + ".json")
    if os.path.exists(path):
        with open(path) as f:
            return json.load(f)
    for url in MIRRORS:
        try:
            req = urllib.request.Request(url, data=urllib.parse.urlencode({"data": q}).encode(),
                                         headers={"User-Agent": "ROTOS-dev/0.1 (personal project)"})
            data = json.load(urllib.request.urlopen(req, timeout=100))
            os.makedirs(cache_dir, exist_ok=True)
            with open(path, "w") as f:
                json.dump(data, f)
            return data
        except Exception as e:  # noqa: BLE001 — try the next mirror
            print(f"overpass {url}: {e}")
    return None


class _Geo:
    """Local metres around the route, good enough over a city."""
    def __init__(self, lat0, lon0):
        self.lat0, self.lon0, self.k = lat0, lon0, math.cos(math.radians(lat0)) * 111320.0

    def xy(self, lat, lon):
        return (lon - self.lon0) * self.k, (lat - self.lat0) * 110540.0


def _seg_dist(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / max(dx * dx + dy * dy, 1e-9)))
    return math.hypot(p[0] - a[0] - dx * t, p[1] - a[1] - dy * t), (dx, dy)


def _angle(u, v):
    nu, nv = math.hypot(*u), math.hypot(*v)
    if nu < 1e-6 or nv < 1e-6:
        return 180.0
    c = max(-1.0, min(1.0, (u[0] * v[0] + u[1] * v[1]) / (nu * nv)))
    return math.degrees(math.acos(c))


def annotate(route, cache_dir):
    """Sets m.hint on route.maneuvers (adding synthetic ones); returns a source label."""
    for m in route.maneuvers:
        m.hint = NONE
    data = fetch_bridges(route, cache_dir)
    if data is None:
        _by_name(route)
        return "road names (Overpass unavailable)"

    geo = _Geo(*route.points[0])
    line = Polyline(route)
    bridges = []
    for w in data.get("elements", []):
        pts = [geo.xy(g["lat"], g["lon"]) for g in w.get("geometry", [])]
        length = sum(math.dist(a, b) for a, b in zip(pts, pts[1:]))
        if len(pts) >= 2 and length >= MIN_BRIDGE_M:
            bridges.append(pts)

    def pos(s):
        lat, lon, _ = line.at(s)
        return geo.xy(lat, lon)

    def heading(s):
        a, b = pos(s - 5), pos(s + 5)
        return b[0] - a[0], b[1] - a[1]

    def on_bridge(s):
        p, h = pos(s), heading(s)
        for br in bridges:
            for a, b in zip(br, br[1:]):
                d, v = _seg_dist(p, a, b)
                # parallel only: passing *under* a crossing flyover does not count
                if d < 8 and min(_angle(h, v), _angle(h, (-v[0], -v[1]))) < 25:
                    return True
        return False

    def start_near(s):
        """A bridge begins within 35 m of the route here, heading our way."""
        p, h = pos(s), heading(s)
        for br in bridges:
            for end, nxt in ((br[0], br[1]), (br[-1], br[-2])):
                if math.dist(p, end) < 35 and _angle(h, (nxt[0] - end[0], nxt[1] - end[1])) < 30:
                    return True
        return False

    # Sample the whole route once every 5 m; everything below is lookups.
    total = route.total_m
    STEP = 5
    n = int(total // STEP) + 1
    ob = [on_bridge(i * STEP) for i in range(n)]
    sn = [start_near(i * STEP) for i in range(n)]

    def idx(a, b):
        return range(max(int(a // STEP), 0), min(int(b // STEP), n))

    def frac(a, b):
        r = idx(a, b)
        return sum(ob[i] for i in r) / len(r) if len(r) else 0.0

    def flyover_starts_here(s0, s1):
        return any(sn[i] for i in idx(s0, s1))

    def decide(a):
        before, after = frac(a - 60, a - 10) > 0.5, frac(a + 20, a + 150) > 0.5
        if after and not before:
            return TAKE
        if before and not after:
            return EXIT
        if not before and not after and flyover_starts_here(a - 10, a + 60):
            return STAY
        return NONE

    for m in route.maneuvers:
        if m.type not in DECISION:
            m.hint = decide(m.along_m)

    # Decision points Google does not announce at all.
    announced = [m.along_m for m in route.maneuvers if m.type not in DECISION]
    extra, last = [], -1e9
    for s in range(50, int(total) - 150, 10):
        if s - last < 200 or any(abs(s - a) < 120 for a in announced):
            continue
        h = NONE
        if frac(s - 40, s - 5) < 0.3 and frac(s + 10, s + 120) > 0.7:
            h = TAKE
        elif frac(s - 40, s + 120) < 0.2 and flyover_starts_here(s, s + 10):
            h = STAY
        if h:
            road = next((m.road for m in reversed(route.maneuvers) if m.along_m <= s), "")
            extra.append(Maneuver(M["STRAIGHT"], float(s), road, NAMES[h]))
            extra[-1].hint = h
            last = s
    route.maneuvers = sorted(route.maneuvers + extra, key=lambda m: m.along_m)
    return f"OSM bridges ({len(bridges)} flyovers nearby)"


def _by_name(route):
    prev = ""
    for m in route.maneuvers:
        here, was = "FLYOVER" in m.road.upper(), "FLYOVER" in prev.upper()
        if m.type not in DECISION:
            m.hint = TAKE if here and not was else (EXIT if was and not here else NONE)
        prev = m.road or prev
