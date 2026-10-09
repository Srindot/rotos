"""Fetch and stitch OpenStreetMap raster tiles, with an on-disk cache.

Respect the OSM tile usage policy: identify yourself, cache, and never bulk
download. A single route corridor is a few hundred tiles at most.
https://operations.osmfoundation.org/policies/tiles/
"""
import io
import os
import time
import urllib.request

import numpy as np
from PIL import Image

URL = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
UA = "ROTOS-dev/0.1 (personal motorcycle navigation project)"


def fetch(z, x, y, cache_dir):
    path = os.path.join(cache_dir, str(z), str(x), f"{y}.png")
    if not os.path.exists(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        req = urllib.request.Request(URL.format(z=z, x=x, y=y), headers={"User-Agent": UA})
        data = urllib.request.urlopen(req, timeout=30).read()
        with open(path, "wb") as f:
            f.write(data)
        time.sleep(0.05)  # be polite
    return np.asarray(Image.open(path).convert("RGB"))


def stitch(z, tiles, x0, y0, w, h, cache_dir, progress=None):
    """Stitch the given set of (tx, ty) tiles into an RGB image whose top-left
    is world tile (x0, y0). Tiles not in the set stay black."""
    img = np.zeros((h * 256, w * 256, 3), np.uint8)
    for n, (tx, ty) in enumerate(sorted(tiles)):
        img[(ty - y0) * 256:(ty - y0 + 1) * 256, (tx - x0) * 256:(tx - x0 + 1) * 256] = \
            fetch(z, tx, ty, cache_dir)
        if progress:
            progress(n + 1, len(tiles))
    return img
