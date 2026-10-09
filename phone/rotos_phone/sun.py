"""Sun elevation, so the phone can tell ROTOS when to switch to night mode.

Low-precision solar position (accurate to well under a degree), plenty for
deciding day vs night.
"""
import math
from datetime import datetime, timedelta, timezone

# The sun's disc touches the horizon at -0.833 deg (refraction + radius).
HORIZON_DEG = -0.833


def elevation_deg(lat, lon, utc):
    d = (utc - datetime(2000, 1, 1, 12, tzinfo=timezone.utc)).total_seconds() / 86400.0
    g = math.radians(357.529 + 0.98560028 * d)
    q = 280.459 + 0.98564736 * d
    L = math.radians(q + 1.915 * math.sin(g) + 0.020 * math.sin(2 * g))
    e = math.radians(23.439 - 0.00000036 * d)
    ra = math.atan2(math.cos(e) * math.sin(L), math.cos(L))
    dec = math.asin(math.sin(e) * math.sin(L))
    gmst_h = (18.697374558 + 24.06570982441908 * d) % 24
    ha = math.radians(gmst_h * 15 + lon) - ra
    la = math.radians(lat)
    return math.degrees(math.asin(math.sin(la) * math.sin(dec) + math.cos(la) * math.cos(dec) * math.cos(ha)))


def is_night(lat, lon, utc):
    return elevation_deg(lat, lon, utc) < HORIZON_DEG


def sunset(lat, lon, day_local, tz):
    """Local sunset time on that date (first day->night crossing after noon)."""
    t = day_local.replace(hour=12, minute=0, second=0, microsecond=0, tzinfo=tz)
    for _ in range(12 * 60):
        if is_night(lat, lon, t.astimezone(timezone.utc)):
            return t
        t += timedelta(minutes=1)
    return None
