"""OSM bridges near a route (Overpass API), cached on disk, for flyover hints."""
import hashlib
import json
import os
import urllib.parse
import urllib.request

MIRRORS = ["https://overpass.private.coffee/api/interpreter",
           "https://overpass-api.de/api/interpreter",
           "https://overpass.kumi.systems/api/interpreter"]
ROADS = "motorway|trunk|primary|secondary|tertiary|unclassified|residential"
UA = "ROTOS-sim/0.2 (personal motorcycle navigation project)"


def query_for(points):
    lats = [p[0] for p in points]
    lons = [p[1] for p in points]
    bbox = f"{min(lats) - .005:.5f},{min(lons) - .005:.5f},{max(lats) + .005:.5f},{max(lons) + .005:.5f}"
    return f'[out:json][timeout:90];way["highway"~"^({ROADS})(_link)?$"]["bridge"="yes"]({bbox});out geom;'


def fetch(points, cache_dir, offline=False):
    """List of bridges, each a list of (lat, lon); None when unavailable."""
    q = query_for(points)
    path = os.path.join(cache_dir, "overpass_" + hashlib.sha1(q.encode()).hexdigest()[:12] + ".json")
    data = None
    if os.path.exists(path):
        with open(path) as f:
            data = json.load(f)
    elif not offline:
        data = _download(q, path)
    if data is None:
        return None
    return [[(g["lat"], g["lon"]) for g in w.get("geometry", [])] for w in data.get("elements", [])]


def _download(q, path):
    for url in MIRRORS:
        try:
            req = urllib.request.Request(url, data=urllib.parse.urlencode({"data": q}).encode(),
                                         headers={"User-Agent": UA})
            data = json.load(urllib.request.urlopen(req, timeout=100))
        except (OSError, ValueError) as e:
            print(f"overpass {url}: {e}")
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(data, f)
        return data
    return None
