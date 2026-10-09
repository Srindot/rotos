"""Current weather from Open-Meteo (free, no API key), for the dashboard.
https://open-meteo.com/en/docs
"""
import json
import urllib.request

URL = ("https://api.open-meteo.com/v1/forecast?latitude={lat:.4f}&longitude={lon:.4f}"
       "&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m,is_day"
       "&timezone=auto&start_date={date}&end_date={date}")


def at(lat, lon, local_dt):
    """Conditions for the hour containing local_dt, or None if offline."""
    try:
        req = urllib.request.Request(URL.format(lat=lat, lon=lon, date=local_dt.strftime("%Y-%m-%d")),
                                     headers={"User-Agent": "ROTOS-dev/0.1"})
        h = json.load(urllib.request.urlopen(req, timeout=20))["hourly"]
        i = local_dt.hour
        return {"temp_c": h["temperature_2m"][i], "code": h["weather_code"][i],
                "rain_prob": h["precipitation_probability"][i] or 0,
                "wind_kmh": h["wind_speed_10m"][i], "is_day": bool(h["is_day"][i])}
    except Exception as e:  # noqa: BLE001 — weather is optional
        print(f"weather unavailable ({e})")
        return None
