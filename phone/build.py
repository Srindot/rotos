"""Phone-side build: Google route + OSM raster -> ROTOS map pack + ride feed.

    python3 phone/build.py --route phone/data/google_route_mock.json --out out

Outputs
  out/route.rmap     quantized map tiles + route + maneuvers  (ROUTE_UPDATE)
  out/ride.rfeed     timestamped phone link capture: PHONE_STATUS + WEATHER
                     throughout, NAVIGATION_STATE during the ride
  out/preview.png    the fused, quantized corridor (debug view)
"""
import argparse
import json
import math
import random
from datetime import datetime, timedelta, timezone
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rotos_phone import (flyover, geo, google_route, mappack, osm_tiles, protocol,  # noqa: E402
                         quantize, ride_sim, sun, weather)

HERE = os.path.dirname(os.path.abspath(__file__))

# Night theme colours, only for the preview image (the device has its own copy)
PREVIEW = ["#11111b", "#181825", "#1b2e26", "#1a2340", "#262637", "#07070c", "#45475a", "#7f849c"]


def hex_rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--route", default=os.path.join(HERE, "data", "google_route_mock.json"))
    ap.add_argument("--out", default=os.path.join(HERE, "..", "out"))
    ap.add_argument("--zoom", type=int, default=16, help="display zoom (tiles fetched at zoom+1)")
    ap.add_argument("--buffer", type=int, default=260, help="corridor half-width, px")
    ap.add_argument("--start", default="sunset-4",
                    help="ride start: HH:MM local, or sunset-N / sunset+N minutes (default shows the day->night switch)")
    ap.add_argument("--date", default=datetime.now().strftime("%Y-%m-%d"), help="ride date YYYY-MM-DD")
    ap.add_argument("--tz", type=float, default=5.5, help="UTC offset, hours (India = 5.5)")
    ap.add_argument("--cache", default=os.path.join(HERE, "cache", "osm"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    with open(a.route) as f:
        route = google_route.parse(json.load(f))
    print(f"route: {route.total_m:.0f} m, {len(route.points)} points, {len(route.maneuvers)} maneuvers")
    src = flyover.annotate(route, os.path.join(HERE, "cache"))
    hints = [m for m in route.maneuvers if m.hint]
    print(f"flyover hints from {src}: " +
          (", ".join(f"{flyover.NAMES[m.hint]} @ {m.along_m:.0f} m" for m in hints) or "none"))

    z = a.zoom
    rpx = [geo.world_px(lat, lon, z) for lat, lon in route.points]
    xs, ys = [p[0] for p in rpx], [p[1] for p in rpx]
    T = mappack.TILE
    ox = int(math.floor((min(xs) - a.buffer) / T) * T)
    oy = int(math.floor((min(ys) - a.buffer) / T) * T)
    tw = int(math.ceil((max(xs) + a.buffer - ox) / T))
    th = int(math.ceil((max(ys) + a.buffer - oy) / T))
    keep = mappack.corridor_cells(rpx, a.buffer, (ox, oy), tw, th)
    print(f"corridor: {keep.sum()} of {tw * th} cells ({T}x{T} px)")

    # OSM tiles at zoom+1 covering the kept cells
    need = set()
    for cy, cx in zip(*np.nonzero(keep)):
        x0, y0 = (ox + cx * T) * 2, (oy + cy * T) * 2
        for tx in range(x0 // 256, (x0 + 2 * T - 1) // 256 + 1):
            for ty in range(y0 // 256, (y0 + 2 * T - 1) // 256 + 1):
                need.add((tx, ty))
    tx0, ty0 = min(t[0] for t in need), min(t[1] for t in need)
    tx1, ty1 = max(t[0] for t in need), max(t[1] for t in need)

    def progress(n, total):
        if sys.stdout.isatty():
            print(f"\rOSM tiles z{z + 1}: {n}/{total}", end="", flush=True)
        elif n == total:
            print(f"OSM tiles z{z + 1}: {total}", end="")
    rgb = osm_tiles.stitch(z + 1, need, tx0, ty0, tx1 - tx0 + 1, ty1 - ty0 + 1, a.cache, progress)
    print()

    cls = quantize.semantic_map(rgb)              # now at zoom z
    # crop to the pack's cell grid
    sx, sy = ox - tx0 * 128, oy - ty0 * 128
    grid = np.zeros((th * T, tw * T), np.uint8)
    h = min(grid.shape[0], cls.shape[0] - sy)
    w = min(grid.shape[1], cls.shape[1] - sx)
    grid[:h, :w] = cls[sy:sy + h, sx:sx + w]

    rmap = os.path.join(a.out, "route.rmap")
    size, slots = mappack.write(rmap, z, (ox, oy), grid, keep, route, rpx)
    print(f"{rmap}: {size / 1024:.0f} KiB, {slots} tiles stored")
    hist = np.bincount(grid[np.repeat(np.repeat(keep, T, 0), T, 1)], minlength=8)
    print("classes:", ", ".join(f"{n} {c / hist.sum():.0%}" for n, c in zip(quantize.NAMES, hist)))

    tz = timezone(timedelta(hours=a.tz))
    day = datetime.strptime(a.date, "%Y-%m-%d").replace(tzinfo=tz)
    lat0, lon0 = route.points[0]
    ss = sun.sunset(lat0, lon0, day, tz)
    if a.start.startswith("sunset"):
        start = ss + timedelta(minutes=int(a.start[len("sunset"):] or 0))   # "sunset-4" -> -4 min
    else:
        hh, mm = map(int, a.start.split(":"))
        start = day.replace(hour=hh, minute=mm)
    print(f"sunset {ss:%H:%M}, ride starts {start:%H:%M} on {a.date}")
    # Link timeline: dashboard first, then the ride, then the dashboard again.
    IDLE_S, POST_S = 15.0, 45.0
    nav, dur = ride_sim.simulate(route, start, t0_s=IDLE_S)
    wx = weather.at(lat0, lon0, start)
    if wx:
        print(f"weather at {start:%H:%M}: {wx['temp_c']:.0f} C, code {wx['code']}, rain {wx['rain_prob']}%")
    status = ride_sim.phone_timeline(start, IDLE_S, IDLE_S + dur + POST_S, lat0, lon0, wx)
    feed = protocol.capture(nav + status, random.Random(3))
    rfeed = os.path.join(a.out, "ride.rfeed")
    with open(rfeed, "wb") as f:
        f.write(feed)
    print(f"{rfeed}: {IDLE_S:.0f} s idle + {dur / 60:.1f} min ride + {POST_S:.0f} s idle, {len(feed)} bytes")

    pal = np.array([hex_rgb(c) for c in PREVIEW], np.uint8)
    img = pal[grid]
    mask = np.repeat(np.repeat(keep, T, 0), T, 1)
    img[~mask] = (0, 0, 0)
    im = Image.fromarray(img)
    d = ImageDraw.Draw(im)
    d.line([(x - ox, y - oy) for x, y in rpx], fill="#33ccff", width=5)
    for m in route.maneuvers:
        i = min(range(len(route.along)), key=lambda k: abs(route.along[k] - m.along_m))
        x, y = rpx[i][0] - ox, rpx[i][1] - oy
        d.rectangle([x - 3, y - 3, x + 3, y + 3], fill="#00ff99")
    im.save(os.path.join(a.out, "preview.png"))
    print("preview: out/preview.png")


if __name__ == "__main__":
    main()
