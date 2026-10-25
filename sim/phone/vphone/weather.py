"""Weather from Open-Meteo (free, no key) for the hour of the ride."""
import json
import urllib.request

URL = ("https://api.open-meteo.com/v1/forecast?latitude={lat:.4f}&longitude={lon:.4f}"
       "&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m,is_day"
       "&timezone=auto&start_date={date}&end_date={date}")
FALLBACK = {"temp_c": 27.0, "code": 2, "rain_prob": 10, "wind_kmh": 8, "is_day": True}


def at(lat, lon, local_dt, offline=False):
    """Conditions for the hour containing local_dt (dict), FALLBACK offline."""
    if offline:
        return dict(FALLBACK)
    try:
        url = URL.format(lat=lat, lon=lon, date=local_dt.strftime("%Y-%m-%d"))
        req = urllib.request.Request(url, headers={"User-Agent": "ROTOS-sim/0.2"})
        h = json.load(urllib.request.urlopen(req, timeout=20))["hourly"]
    except (OSError, ValueError, KeyError) as e:
        print(f"weather unavailable ({e}); using a fixed sample")
        return dict(FALLBACK)
    i = local_dt.hour
    return {"temp_c": float(h["temperature_2m"][i]), "code": int(h["weather_code"][i]),
            "rain_prob": int(h["precipitation_probability"][i] or 0),
            "wind_kmh": int(h["wind_speed_10m"][i]), "is_day": bool(h["is_day"][i])}


FORECAST_URL = ("https://api.open-meteo.com/v1/forecast?latitude={lat:.4f}&longitude={lon:.4f}"
                "&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m"
                "&daily=sunrise,sunset&timezone=auto&start_date={d0}&end_date={d1}")


def _minutes(iso_time):
    """'2026-10-10T17:59' -> 1079."""
    hh, mm = iso_time.split("T")[1].split(":")[:2]
    return int(hh) * 60 + int(mm)


def _fallback_forecast(local_dt):
    hours = [(29.0 - i, 61 if i in (2, 3) else (3 if i < 2 else 2), 70 if i in (2, 3) else 15, 10) for i in range(8)]
    return {"start_hour": local_dt.hour, "sunrise_min": 6 * 60 + 5, "sunset_min": 17 * 60 + 59, "hours": hours}


def forecast(lat, lon, local_dt, offline=False):
    """Next 8 hours from local_dt's hour + today's sunrise/sunset:
    {start_hour, sunrise_min, sunset_min, hours: [(temp_c, code, rain, wind)]}."""
    if offline:
        return _fallback_forecast(local_dt)
    from datetime import timedelta
    d0, d1 = local_dt.strftime("%Y-%m-%d"), (local_dt + timedelta(days=1)).strftime("%Y-%m-%d")
    try:
        req = urllib.request.Request(FORECAST_URL.format(lat=lat, lon=lon, d0=d0, d1=d1),
                                     headers={"User-Agent": "ROTOS-sim/0.2"})
        data = json.load(urllib.request.urlopen(req, timeout=20))
        h, day = data["hourly"], data["daily"]
        i0 = local_dt.hour
        hours = [(float(h["temperature_2m"][i]), int(h["weather_code"][i]),
                  int(h["precipitation_probability"][i] or 0), int(h["wind_speed_10m"][i]))
                 for i in range(i0, i0 + 8)]
        return {"start_hour": i0, "sunrise_min": _minutes(day["sunrise"][0]),
                "sunset_min": _minutes(day["sunset"][0]), "hours": hours}
    except (OSError, ValueError, KeyError, IndexError) as e:
        print(f"forecast unavailable ({e}); using a fixed sample")
        return _fallback_forecast(local_dt)
