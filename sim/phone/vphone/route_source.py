"""Routes in Google Routes API v2 shape: a saved response, the live API
(needs MAPS_API_KEY), or OSRM converted to the same shape."""
import json
import urllib.request
import zlib

from .osrm import osrm_to_google

GOOGLE_URL = "https://routes.googleapis.com/directions/v2:computeRoutes"
GOOGLE_FIELDS = ("routes.duration,routes.distanceMeters,routes.polyline.encodedPolyline,"
                 "routes.legs.steps.polyline.encodedPolyline,routes.legs.steps.navigationInstruction,"
                 "routes.legs.steps.distanceMeters")
UA = "ROTOS-sim/0.2 (personal motorcycle navigation project)"


def load_file(path):
    with open(path) as f:
        return json.load(f)


def fetch_google(origin, dest, api_key, timeout=30):
    body = {
        "origin": {"location": {"latLng": {"latitude": origin[0], "longitude": origin[1]}}},
        "destination": {"location": {"latLng": {"latitude": dest[0], "longitude": dest[1]}}},
        "travelMode": "TWO_WHEELER", "languageCode": "en",
    }
    req = urllib.request.Request(GOOGLE_URL, data=json.dumps(body).encode(), method="POST", headers={
        "Content-Type": "application/json", "X-Goog-Api-Key": api_key, "X-Goog-FieldMask": GOOGLE_FIELDS})
    return json.load(urllib.request.urlopen(req, timeout=timeout))


def fetch_osrm(origin, dest, timeout=30):
    """OSRM demo server -> Google-shaped response."""
    url = ("https://router.project-osrm.org/route/v1/driving/"
           f"{origin[1]},{origin[0]};{dest[1]},{dest[0]}?overview=false&steps=true&geometries=polyline")
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    return osrm_to_google(json.load(urllib.request.urlopen(req, timeout=timeout)))


def steps_of(resp):
    route = resp["routes"][0]
    return [s for leg in route["legs"] for s in leg["steps"]], float(route.get("duration", "0s").rstrip("s"))


def route_id_of(resp):
    """Stable, non-zero id: CRC-32 of all step polylines."""
    steps, _ = steps_of(resp)
    crc = 0
    for s in steps:
        crc = zlib.crc32(s["polyline"]["encodedPolyline"].encode(), crc)
    return crc or 1


def load_into(nav, resp):
    """Feeds a Google-shaped route into the navigation library."""
    steps, duration = steps_of(resp)
    nav.reset()
    for s in steps:
        first = nav.add_polyline(s["polyline"]["encodedPolyline"])
        ni = s.get("navigationInstruction", {})
        nav.add_maneuver_google(ni.get("maneuver", "STRAIGHT"), first, ni.get("instructions", ""))
    rid = route_id_of(resp)
    nav.finish_route(duration, rid)
    return {"route_id": rid, "duration_s": duration, "total_m": nav.total_m(), "points": nav.point_count()}
