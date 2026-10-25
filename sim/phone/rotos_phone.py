"""rotos_phone - the virtual phone for the ROTOS simulator.

Does what the Android app does, on the desktop, with the same C navigation
library: route -> flyover hints -> map pack from OSM -> weather -> a
simulated ride -> packets. Either writes a capture for replay or talks to a
running simulator live.

    python3 sim/phone/rotos_phone.py                       # -> out/ride.rfeed
    ./build/host/sim/rotos_sim --feed out/ride.rfeed       # replay it

    ./build/host/sim/rotos_sim --listen 7400 &             # or live:
    python3 sim/phone/rotos_phone.py --live 127.0.0.1:7400

Steps, in order: navigation library -> route -> flyover hints -> map pack
-> clock and weather -> simulated ride -> capture or live session -> preview.
"""
import argparse
import os
import random
import sys
from datetime import datetime, timedelta, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "core", "python"))
sys.path.insert(0, HERE)

import rotos_nav as rn  # noqa: E402
from vphone import feed, live, mapbuild, osm_tiles, overpass, preview, route_source, timeline, weather  # noqa: E402

IDLE_MS = 15000          # dashboard time before the ride
POST_MS = 45000          # dashboard time after it


def parse_args():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group()
    src.add_argument("--route", default=os.path.join(HERE, "data", "google_route_mock.json"),
                     help="saved Google Routes API v2 response")
    src.add_argument("--osrm", nargs=2, metavar=("FROM", "TO"), help="lat,lon lat,lon via OSRM")
    src.add_argument("--google", nargs=2, metavar=("FROM", "TO"), help="lat,lon lat,lon via Google (MAPS_API_KEY)")
    ap.add_argument("--out", default=os.path.join(ROOT, "out"))
    ap.add_argument("--start", default="sunset-4", help="HH:MM local, or sunset-N / sunset+N minutes")
    ap.add_argument("--date", default=datetime.now().strftime("%Y-%m-%d"))
    ap.add_argument("--tz", type=float, default=5.5, help="UTC offset in hours (India 5.5)")
    ap.add_argument("--rate", type=int, default=20000, help="map transfer bytes/s (BLE ~ 20000)")
    ap.add_argument("--cruise", type=float, default=12.0, help="rider cruise speed m/s")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--google-cap", action="store_true", help="phone reports its Google usage cap (ROTOS shows G!)")
    ap.add_argument("--offline", action="store_true", help="cached tiles / bridges only, fixed weather")
    ap.add_argument("--live", metavar="HOST:PORT", help="talk to rotos_sim --listen instead of writing a feed")
    ap.add_argument("--speed", type=float, default=1.0, help="live mode: time multiplier")
    ap.add_argument("--cache", default=os.path.join(HERE, "cache"))
    return ap.parse_args()


def load_route(nav, a):
    if a.osrm:
        resp = route_source.fetch_osrm(*[tuple(map(float, p.split(","))) for p in a.osrm])
    elif a.google:
        key = os.environ.get("MAPS_API_KEY", "")
        if not key:
            sys.exit("--google needs MAPS_API_KEY in the environment")
        resp = route_source.fetch_google(*[tuple(map(float, p.split(","))) for p in a.google], key)
    else:
        resp = route_source.load_file(a.route)
    info = route_source.load_into(nav, resp)
    print(f"route {info['route_id']:08x}: {info['total_m']:.0f} m, {info['points']} points, "
          f"{len(nav.maneuvers())} maneuvers, {info['duration_s'] / 60:.0f} min by the router")
    return info


def add_flyover_hints(nav, a):
    pts = [nav.point(i)[:2] for i in range(nav.point_count())]
    bridges = overpass.fetch(pts, a.cache, a.offline)
    if bridges is None:
        n, src = nav.flyover_by_name(), "road names (Overpass unavailable)"
    else:
        for br in bridges:
            nav.bridges_add(br)
        n, src = nav.flyover_annotate(), f"OSM bridges ({nav.bridges_count()} flyovers nearby)"
    hints = [f"{rn.FLYOVER_NAMES[m['hint']]} @ {m['along_m']:.0f} m" for m in nav.maneuvers() if m["hint"]]
    print(f"flyover hints from {src}: {', '.join(hints) or 'none'} ({n})")


def build_pack(nav, a):
    tiles = osm_tiles.TileStore(os.path.join(a.cache, "osm"), a.offline)

    def progress(i, n):
        if sys.stdout.isatty():
            print(f"\rmap blocks {i}/{n}", end="", flush=True)
    pack, grid, info = mapbuild.build(nav, tiles, progress)
    if sys.stdout.isatty():
        print()
    print(f"map pack: {len(pack) / 1024:.0f} KiB ({info['wire'] / 1024:.0f} KiB compressed on the link), {info['n_keep']} tiles in {info['blocks']} blocks, "
          f"OSM tiles fetched {tiles.fetched}, missing {tiles.missing}")
    with open(os.path.join(a.out, "route.rmap"), "wb") as f:
        f.write(pack)
    return pack, grid, info


def ride_clock(nav, a):
    tz = timezone(timedelta(hours=a.tz))
    day = datetime.strptime(a.date, "%Y-%m-%d").replace(tzinfo=tz)
    lat0, lon0, _ = nav.point(0)
    if a.start.startswith("sunset"):
        sunset = datetime.fromtimestamp(nav.next_sunset(lat0, lon0, day.replace(hour=12).timestamp()), tz)
        start = sunset + timedelta(minutes=int(a.start[len("sunset"):] or 0))
        print(f"sunset {sunset:%H:%M}, ride starts {start:%H:%M} on {a.date}")
    else:
        hh, mm = map(int, a.start.split(":"))
        start = day.replace(hour=hh, minute=mm)
    return start, int(a.tz * 3600)


def main():
    a = parse_args()
    os.makedirs(a.out, exist_ok=True)
    nav = rn.Nav()
    load_route(nav, a)
    add_flyover_hints(nav, a)
    pack, grid, info = build_pack(nav, a)
    start, utc_off = ride_clock(nav, a)
    lat0, lon0, _ = nav.point(0)
    wx = weather.at(lat0, lon0, start, a.offline)
    print(f"weather: {wx['temp_c']:.0f} C, WMO code {wx['code']}, rain {wx['rain_prob']}%")
    fc = weather.forecast(lat0, lon0, start, a.offline)
    print("forecast: " + " ".join(f"{(fc['start_hour'] + i) % 24:02d}h {h[0]:.0f}C/{h[2]}%"
                                  for i, h in enumerate(fc["hours"][:6])))
    fixes = timeline.simulate_ride(nav, a.seed, a.cruise)
    ride_s = fixes[-1][0]
    if a.live:
        host, port = a.live.rsplit(":", 1)
        clock = timeline.Clock(start, 0, utc_off)
        live.run(nav, host, int(port), nav.wire_len(), fixes, clock, wx, a.speed, fc)
    else:
        _, _, map_end = timeline.map_transfer_times(nav.wire_len(), nav.map_chunk_size(), a.rate)
        ride_t0 = max(IDLE_MS, map_end + 2000)
        clock = timeline.Clock(start, ride_t0, utc_off)
        end_ms = ride_t0 + int(ride_s * 1000) + POST_MS
        frames = timeline.build(nav, nav.wire_len(), fixes, clock, wx, end_ms, a.rate, fc=fc, google_cap=a.google_cap)
        data = feed.encode(feed.deliver(frames, random.Random(3)))
        with open(os.path.join(a.out, "ride.rfeed"), "wb") as f:
            f.write(data)
        print(f"out/ride.rfeed: map transfer {map_end / 1000:.1f} s, ride {ride_s / 60:.1f} min from "
              f"t={ride_t0 / 1000:.0f} s, {len(frames)} packets, {len(data) / 1024:.0f} KiB")
        presses = ",".join(f"A@{t}" for t in timeline.press_times(clock, 2600))
        print(f"scripted calls: replay with `rotos_sim --feed out/ride.rfeed --press {presses}` "
              f"to answer them like a rider would")
    preview.render(grid, info, nav, mapbuild.ZOOM, os.path.join(a.out, "preview.png"))
    print(f"preview: {os.path.join(a.out, 'preview.png')} | C assertion failures: {nav.assert_count()}")


if __name__ == "__main__":
    main()
