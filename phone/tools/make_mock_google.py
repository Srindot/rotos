"""Generate a mock Google Routes API (v2 computeRoutes) response.

We are not integrating with Google yet, but the rest of the pipeline should
only ever see Google-shaped data. So we ask the public OSRM demo server for a
real road route once and re-shape it into the Routes API JSON format.

    python3 phone/tools/make_mock_google.py \
        --from 17.4455,78.3489 --to 17.4344,78.3866 \
        --out phone/data/google_route_mock.json
"""
import argparse
import json
import urllib.request

OSRM = "https://router.project-osrm.org/route/v1/driving/{lon1},{lat1};{lon2},{lat2}" \
       "?overview=full&geometries=polyline&steps=true"

# OSRM (type, modifier) -> Google Routes API Maneuver enum
def to_google_maneuver(m):
    t, mod = m.get("type"), m.get("modifier", "straight")
    side = "LEFT" if "left" in mod else "RIGHT"
    if t == "depart":
        return "DEPART"
    if t in ("roundabout", "rotary", "roundabout turn", "exit roundabout", "exit rotary"):
        return "ROUNDABOUT_" + side
    if t in ("fork",):
        return "FORK_" + side
    if t in ("merge",):
        return "MERGE"
    if t in ("on ramp", "off ramp"):
        return "RAMP_" + side
    if t == "new name":
        return "NAME_CHANGE"
    return {
        "uturn": "UTURN_" + side,
        "sharp left": "TURN_SHARP_LEFT", "sharp right": "TURN_SHARP_RIGHT",
        "left": "TURN_LEFT", "right": "TURN_RIGHT",
        "slight left": "TURN_SLIGHT_LEFT", "slight right": "TURN_SLIGHT_RIGHT",
        "straight": "STRAIGHT",
    }.get(mod, "STRAIGHT")

VERB = {
    "DEPART": "Head", "NAME_CHANGE": "Continue", "STRAIGHT": "Continue straight",
    "TURN_LEFT": "Turn left", "TURN_RIGHT": "Turn right",
    "TURN_SLIGHT_LEFT": "Slight left", "TURN_SLIGHT_RIGHT": "Slight right",
    "TURN_SHARP_LEFT": "Sharp left", "TURN_SHARP_RIGHT": "Sharp right",
    "UTURN_LEFT": "Make a U-turn", "UTURN_RIGHT": "Make a U-turn",
    "ROUNDABOUT_LEFT": "At the roundabout, take the exit", "ROUNDABOUT_RIGHT": "At the roundabout, take the exit",
    "FORK_LEFT": "Keep left at the fork", "FORK_RIGHT": "Keep right at the fork",
    "RAMP_LEFT": "Take the ramp on the left", "RAMP_RIGHT": "Take the ramp on the right",
    "MERGE": "Merge",
}

def latlng(lon, lat):
    return {"latLng": {"latitude": lat, "longitude": lon}}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="src", required=True, help="lat,lon")
    ap.add_argument("--to", dest="dst", required=True, help="lat,lon")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    lat1, lon1 = map(float, a.src.split(","))
    lat2, lon2 = map(float, a.dst.split(","))

    req = urllib.request.Request(OSRM.format(lat1=lat1, lon1=lon1, lat2=lat2, lon2=lon2),
                                 headers={"User-Agent": "rotos-dev/0.1"})
    osrm = json.load(urllib.request.urlopen(req, timeout=30))
    r = osrm["routes"][0]

    steps = []
    for leg in r["legs"]:
        for s in leg["steps"]:
            if s["maneuver"]["type"] == "arrive":
                continue  # Routes API has no explicit arrive step
            man = to_google_maneuver(s["maneuver"])
            name = s.get("name") or ""
            text = VERB[man]
            if name:
                text += (" on " if man in ("DEPART", "NAME_CHANGE", "STRAIGHT") else " onto ") + name
            coords = s["maneuver"]["location"]
            steps.append({
                "distanceMeters": round(s["distance"]),
                "staticDuration": f"{round(s['duration'])}s",
                "polyline": {"encodedPolyline": s["geometry"]},
                "startLocation": latlng(*coords),
                "navigationInstruction": {"maneuver": man, "instructions": text},
                "travelMode": "TWO_WHEELER",
            })
    # endLocation of each step = startLocation of the next
    for i, s in enumerate(steps):
        nxt = steps[i + 1]["startLocation"] if i + 1 < len(steps) else latlng(lon2, lat2)
        s["endLocation"] = nxt

    out = {"routes": [{
        "distanceMeters": round(r["distance"]),
        "duration": f"{round(r['duration'])}s",
        "polyline": {"encodedPolyline": r["geometry"]},
        "legs": [{
            "distanceMeters": round(r["distance"]),
            "duration": f"{round(r['duration'])}s",
            "startLocation": latlng(lon1, lat1),
            "endLocation": latlng(lon2, lat2),
            "steps": steps,
        }],
        "description": "MOCK — generated from OSRM, shaped like Google Routes API v2",
    }]}
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2)
    print(f"{len(steps)} steps, {r['distance']:.0f} m -> {a.out}")

if __name__ == "__main__":
    main()
