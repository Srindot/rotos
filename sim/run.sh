#!/usr/bin/env bash
# ROTOS simulator in one command:
#   build -> virtual phone makes out/ride.rfeed -> screen twin -> rotos_sim replays it
#
#   sim/run.sh                 replay a simulated ride (window)
#   sim/run.sh --live          wait for a live phone on TCP 7400 (Android emulator)
#   sim/run.sh --headless      no window: frames in out/snaps/
# Extra arguments after the mode go to rotos_sim (e.g. --speed 4, --theme day).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
MODE="${1:-replay}"; [ $# -gt 0 ] && shift

cmake -S . -B build/host >/dev/null
cmake --build build/host -j >/dev/null
BIN=build/host/sim

if [ "$MODE" = "--live" ]; then
    "$BIN/rotos_screen" & SCREEN=$!
    trap 'kill $SCREEN 2>/dev/null || true' EXIT
    echo "ROTOS waiting for a phone on TCP 7400 (Android emulator: 10.0.2.2:7400,"
    echo "or: python3 sim/phone/rotos_phone.py --live 127.0.0.1:7400)"
    "$BIN/rotos_sim" --listen 7400 "$@"
    exit 0
fi

[ -f out/ride.rfeed ] || python3 sim/phone/rotos_phone.py
if [ "$MODE" = "--headless" ]; then
    "$BIN/rotos_sim" --headless --fresh --feed out/ride.rfeed \
        --snap 1200,8000,20000,60000,300000,420000,645000 --snap-dir out/snaps "$@"
    exit 0
fi
"$BIN/rotos_screen" & SCREEN=$!
trap 'kill $SCREEN 2>/dev/null || true' EXIT
"$BIN/rotos_sim" --fresh --feed out/ride.rfeed "$@"
