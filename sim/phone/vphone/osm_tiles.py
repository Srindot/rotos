"""OpenStreetMap raster tiles with an on-disk cache.

Tile usage policy: identify yourself, cache, never bulk download. Only the
tiles under a route corridor are ever requested.
https://operations.osmfoundation.org/policies/tiles/
"""
import io
import os
import time
import urllib.request

import numpy as np
from PIL import Image

URL = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
UA = "ROTOS-sim/0.2 (personal motorcycle navigation project)"
TILE = 256


class TileStore:
    def __init__(self, cache_dir, offline=False):
        self.cache_dir = cache_dir
        self.offline = offline
        self.fetched = 0
        self.missing = 0

    def _path(self, z, x, y):
        return os.path.join(self.cache_dir, str(z), str(x), f"{y}.png")

    def tile(self, z, x, y):
        """RGB uint8 array (256, 256, 3), or None when unavailable."""
        path = self._path(z, x, y)
        if not os.path.exists(path):
            if self.offline:
                self.missing += 1
                return None
            try:
                req = urllib.request.Request(URL.format(z=z, x=x, y=y), headers={"User-Agent": UA})
                data = urllib.request.urlopen(req, timeout=30).read()
            except OSError as e:
                print(f"tile {z}/{x}/{y}: {e}")
                self.missing += 1
                return None
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(data)
            self.fetched += 1
            time.sleep(0.05)                       # be polite
        return np.asarray(Image.open(path).convert("RGB"))

    def window(self, z, x0, y0, w, h, wanted=None):
        """RGB window of world pixels [x0, x0+w) x [y0, y0+h) at zoom z.
        wanted(tx, ty) -> bool limits which tiles are fetched; others stay black."""
        out = np.zeros((h, w, 3), np.uint8)
        for ty in range(y0 // TILE, (y0 + h - 1) // TILE + 1):
            for tx in range(x0 // TILE, (x0 + w - 1) // TILE + 1):
                if wanted is not None and not wanted(tx, ty):
                    continue
                img = self.tile(z, tx, ty)
                if img is None:
                    continue
                ax, ay = max(x0, tx * TILE), max(y0, ty * TILE)
                bx, by = min(x0 + w, (tx + 1) * TILE), min(y0 + h, (ty + 1) * TILE)
                out[ay - y0:by - y0, ax - x0:bx - x0] = img[ay - ty * TILE:by - ty * TILE, ax - tx * TILE:bx - tx * TILE]
        return out
