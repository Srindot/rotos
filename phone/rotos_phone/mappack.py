"""ROTOS map pack (.rmap): quantized map tiles + route + maneuvers.

Everything ROTOS needs to draw a route, in one flat little-endian blob that
the ESP32 can keep in flash and read in place. Layout (see core/mappack.h):

  header   'RMAP' u16 version, u8 zoom, u8 tile_size,
           i32 origin_x, i32 origin_y,           world px of tile grid (0,0)
           u16 tiles_w, u16 tiles_h, u16 n_slots, u16 n_maneuvers,
           u32 n_points, f32 metres_per_px, f32 total_m
  index    u16[tiles_w * tiles_h]                slot id or 0xFFFF (not stored)
  tiles    n_slots * (tile_size^2 / 2) bytes     4 bpp, high nibble = left pixel
  points   n_points * (f32 x, f32 y, f32 along)  route in pack pixels
  mans     n_maneuvers * (u8 type, u8 name_len, u8 hint, u8 pad, f32 along, name[name_len])
           hint: 0 none, 1 take flyover, 2 stay below, 3 exit flyover
"""
import struct

import numpy as np

from . import geo

MAGIC = b"RMAP"
VERSION = 1
TILE = 32
NO_TILE = 0xFFFF


def corridor_cells(route_px, buffer_px, origin, tw, th):
    """bool[th, tw]: grid cells whose centre is within buffer of the route."""
    pts = np.asarray(route_px, np.float64)
    a, b = pts[:-1], pts[1:]
    ab = b - a
    ab2 = np.maximum((ab ** 2).sum(1), 1e-9)
    cy, cx = np.mgrid[0:th, 0:tw]
    c = np.stack([origin[0] + (cx + 0.5) * TILE, origin[1] + (cy + 0.5) * TILE], -1).reshape(-1, 2)
    best = np.full(len(c), np.inf)
    for i in range(0, len(a), 256):  # chunk to bound memory
        aa, dd, d2 = a[i:i + 256], ab[i:i + 256], ab2[i:i + 256]
        t = np.clip(((c[:, None] - aa[None]) * dd[None]).sum(2) / d2[None], 0, 1)
        proj = aa[None] + t[..., None] * dd[None]
        best = np.minimum(best, np.sqrt(((c[:, None] - proj) ** 2).sum(2)).min(1))
    return (best < buffer_px + TILE * 0.71).reshape(th, tw)


def ascii_upper(s, n=40):
    s = s.upper().encode("ascii", "ignore").decode()
    return s[:n]


def write(path, zoom, origin, classes, keep, route, route_px):
    th, tw = keep.shape
    index = np.full(th * tw, NO_TILE, np.uint16)
    blobs = []
    for ty in range(th):
        for tx in range(tw):
            if not keep[ty, tx]:
                continue
            t = classes[ty * TILE:(ty + 1) * TILE, tx * TILE:(tx + 1) * TILE].astype(np.uint8)
            index[ty * tw + tx] = len(blobs)
            blobs.append(((t[:, 0::2] << 4) | t[:, 1::2]).tobytes())

    mid = route.points[len(route.points) // 2][0]
    out = bytearray()
    out += MAGIC
    out += struct.pack("<HBBiiHHHHIff", VERSION, zoom, TILE, origin[0], origin[1],
                       tw, th, len(blobs), len(route.maneuvers), len(route.points),
                       geo.meters_per_px(mid, zoom), route.total_m)
    out += index.tobytes()
    for b in blobs:
        out += b
    for (x, y), s in zip(route_px, route.along):
        out += struct.pack("<fff", x - origin[0], y - origin[1], s)
    for m in route.maneuvers:
        name = ascii_upper(m.road).encode()
        out += struct.pack("<BBBBf", m.type, len(name), m.hint, 0, m.along_m) + name
    with open(path, "wb") as f:
        f.write(out)
    return len(out), len(blobs)
