"""OSRM route -> Google Routes API v2 shape, so everything downstream only
ever sees Google-shaped data (and a real Google response drops straight in)."""

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
TURNS = {
    "sharp left": "TURN_SHARP_LEFT", "sharp right": "TURN_SHARP_RIGHT",
    "left": "TURN_LEFT", "right": "TURN_RIGHT",
    "slight left": "TURN_SLIGHT_LEFT", "slight right": "TURN_SLIGHT_RIGHT",
    "straight": "STRAIGHT",
}


def to_google_maneuver(m):
    """OSRM (type, modifier) -> Google Maneuver enum string."""
    t, mod = m.get("type"), m.get("modifier", "straight")
    side = "LEFT" if "left" in mod else "RIGHT"
    if t == "depart":
        return "DEPART"
    if t in ("roundabout", "rotary", "roundabout turn", "exit roundabout", "exit rotary"):
        return "ROUNDABOUT_" + side
    if t == "fork":
        return "FORK_" + side
    if t == "merge":
        return "MERGE"
    if t in ("on ramp", "off ramp"):
        return "RAMP_" + side
    if t == "new name":
        return "NAME_CHANGE"
    if mod == "uturn":
        return "UTURN_" + side
    return TURNS.get(mod, "STRAIGHT")


def _latlng(lon, lat):
    return {"latLng": {"latitude": lat, "longitude": lon}}


def _step(s):
    man = to_google_maneuver(s["maneuver"])
    name = s.get("name") or ""
    text = VERB[man]
    if name:
        text += (" on " if man in ("DEPART", "NAME_CHANGE", "STRAIGHT") else " onto ") + name
    return {
        "distanceMeters": round(s["distance"]),
        "staticDuration": f"{round(s['duration'])}s",
        "polyline": {"encodedPolyline": s["geometry"]},
        "startLocation": _latlng(*s["maneuver"]["location"]),
        "navigationInstruction": {"maneuver": man, "instructions": text},
        "travelMode": "TWO_WHEELER",
    }


def osrm_to_google(osrm):
    r = osrm["routes"][0]
    steps = [_step(s) for leg in r["legs"] for s in leg["steps"] if s["maneuver"]["type"] != "arrive"]
    for i, s in enumerate(steps):               # endLocation = next startLocation
        s["endLocation"] = steps[i + 1]["startLocation"] if i + 1 < len(steps) else s["startLocation"]
    leg = {"distanceMeters": round(r["distance"]), "duration": f"{round(r['duration'])}s", "steps": steps}
    return {"routes": [{
        "distanceMeters": round(r["distance"]),
        "duration": f"{round(r['duration'])}s",
        "legs": [leg],
        "description": "generated from OSRM, shaped like Google Routes API v2",
    }]}
