"""Parse a Google Routes API v2 response into the ROTOS navigation model.

Only Google-shaped input is accepted here, so swapping the mock file for a
real API response later changes nothing downstream.
"""
import re
from dataclasses import dataclass, field

from . import geo, polyline

# Must match core/nav.h
MANEUVER = {
    "NONE": 0, "DEPART": 1, "STRAIGHT": 2,
    "TURN_SLIGHT_LEFT": 3, "TURN_LEFT": 4, "TURN_SHARP_LEFT": 5, "UTURN_LEFT": 6,
    "TURN_SLIGHT_RIGHT": 7, "TURN_RIGHT": 8, "TURN_SHARP_RIGHT": 9, "UTURN_RIGHT": 10,
    "ROUNDABOUT_LEFT": 11, "ROUNDABOUT_RIGHT": 12, "FORK_LEFT": 13, "FORK_RIGHT": 14,
    "MERGE": 15, "RAMP_LEFT": 16, "RAMP_RIGHT": 17, "NAME_CHANGE": 18, "ARRIVE": 19,
}


@dataclass
class Maneuver:
    type: int
    along_m: float      # distance from route start where the maneuver happens
    road: str           # road you are on after the maneuver
    instruction: str
    hint: int = 0       # flyover hint, see flyover.py


@dataclass
class Route:
    points: list        # [(lat, lon)]
    along: list         # cumulative metres at each point
    maneuvers: list = field(default_factory=list)
    total_m: float = 0.0
    duration_s: float = 0.0


_ROAD_RE = re.compile(r"\b(?:onto|on|toward|towards)\s+(.+)$", re.I)


def road_name(instruction):
    m = _ROAD_RE.search(instruction or "")
    return m.group(1).strip() if m else ""


def parse(resp):
    r = resp["routes"][0]
    steps = [s for leg in r["legs"] for s in leg["steps"]]

    pts, starts = [], []
    for s in steps:
        sp = polyline.decode(s["polyline"]["encodedPolyline"])
        if pts and sp and sp[0] == pts[-1]:
            sp = sp[1:]  # consecutive steps share their junction point
        starts.append(max(len(pts) - 1, 0))
        pts.extend(sp)

    along = [0.0]
    for a, b in zip(pts, pts[1:]):
        along.append(along[-1] + geo.haversine(a, b))

    mans = []
    for s, i in zip(steps, starts):
        ni = s.get("navigationInstruction", {})
        t = MANEUVER.get(ni.get("maneuver", "STRAIGHT"), MANEUVER["STRAIGHT"])
        text = ni.get("instructions", "")
        mans.append(Maneuver(t, along[i], road_name(text), text))
    mans.append(Maneuver(MANEUVER["ARRIVE"], along[-1], "", "Arrive at destination"))

    dur = float(r.get("duration", "0s").rstrip("s"))
    return Route(pts, along, mans, along[-1], dur)
