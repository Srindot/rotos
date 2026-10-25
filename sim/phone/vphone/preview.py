"""Debug picture of a map pack: the quantized corridor in the night palette,
route on top, maneuvers as green squares (flyover hints magenta)."""
import math

import numpy as np
from PIL import Image, ImageDraw

NIGHT = ["#11111b", "#181825", "#1b2e26", "#1a2340", "#262637", "#07070c", "#45475a", "#7f849c"]


def _rgb(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


def world_px(lat, lon, z):
    n = 256 * (1 << z)
    s = math.sin(math.radians(lat))
    return (lon + 180.0) / 360.0 * n, (0.5 - math.log((1 + s) / (1 - s)) / (4 * math.pi)) * n


def render(grid, info, nav, zoom, path):
    pal = np.zeros((256, 3), np.uint8)
    for i, c in enumerate(NIGHT):
        pal[i] = _rgb(c)
    im = Image.fromarray(pal[grid])
    d = ImageDraw.Draw(im)
    ox, oy = info["origin_x"], info["origin_y"]
    pts = [world_px(*nav.point(i)[:2], zoom) for i in range(nav.point_count())]
    d.line([(x - ox, y - oy) for x, y in pts], fill="#33ccff", width=5)
    alongs = [nav.point(i)[2] for i in range(nav.point_count())]
    for m in nav.maneuvers():
        lat, lon = _latlon_at(nav, alongs, m["along_m"])
        x, y = world_px(lat, lon, zoom)
        col = "#ff44cc" if m["hint"] else "#00ff99"
        d.rectangle([x - ox - 4, y - oy - 4, x - ox + 4, y - oy + 4], fill=col)
    im.save(path)


def _latlon_at(nav, alongs, s):
    i = max(0, min(len(alongs) - 2, next((k for k in range(len(alongs) - 1) if alongs[k + 1] >= s), len(alongs) - 2)))
    a, b = nav.point(i), nav.point(i + 1)
    t = 0.0 if b[2] <= a[2] else (s - a[2]) / (b[2] - a[2])
    return a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
