"""Save a Google-Routes-shaped route (made from OSRM) for offline runs.

    python3 sim/phone/tools/make_mock_google.py \
        --from 17.4455,78.3489 --to 17.4300,78.3890 --out sim/phone/data/my_route.json
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from vphone import route_source  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="src", required=True, help="lat,lon")
    ap.add_argument("--to", dest="dst", required=True, help="lat,lon")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    resp = route_source.fetch_osrm(tuple(map(float, a.src.split(","))), tuple(map(float, a.dst.split(","))))
    with open(a.out, "w") as f:
        json.dump(resp, f, indent=2)
    steps, _ = route_source.steps_of(resp)
    print(f"{len(steps)} steps -> {a.out}")


if __name__ == "__main__":
    main()
