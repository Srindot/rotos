#!/usr/bin/env bash
# End-to-end demo: fake ROTOS (or the real simulator) on the host, the app in
# the emulator riding the bundled sample route in demo mode over TCP.
#   scripts/demo.sh                 uses android/tools/fake_rotos.py on 127.0.0.1:7400
#   scripts/demo.sh --sim           assume something (rotos_sim) already listens on 7400
#   SPEEDUP=4 scripts/demo.sh       ride faster than real time
#   ROUTE=osrm scripts/demo.sh      plan IIIT -> Durgam Cheruvu via OSRM instead of the sample
# Needs a running emulator (scripts/emulator.sh) and a built APK.
set -euo pipefail
source "$(dirname "$0")/common.sh"
SPEEDUP=${SPEEDUP:-4}
SAMPLE=true; [ "${ROUTE:-sample}" = "osrm" ] && SAMPLE=false
LOG="$OUT_DIR/fake_rotos.log"

# The previous run's app may still be connected (switch on) and would take
# fake ROTOS's only connection: install and stop it before anything listens.
"$ANDROID_DIR/scripts/install.sh" >/dev/null
adb_emu shell am force-stop dev.rotos.app

FAKE_PID=""
if [ "${1:-}" != "--sim" ]; then
    python3 -u "$ANDROID_DIR/tools/fake_rotos.py" --exit-after-arrived 6 --save "$OUT_DIR/received.rmap" \
        --accept-timeout 900 > "$LOG" 2>&1 &
    FAKE_PID=$!
    sleep 1
fi

adb_emu logcat -c
adb_emu shell am start -n dev.rotos.app/.ui.MainActivity \
    --ez demo true --ez sample "$SAMPLE" --es link tcp --es host 10.0.2.2 --ei port 7400 \
    --ef speedup "$SPEEDUP" --ez autostart true >/dev/null
echo "app started (demo, sample=$SAMPLE, x$SPEEDUP); fake ROTOS log: $LOG"

shot=0
for i in $(seq 1 600); do
    sleep 5
    if [ $((i % 6)) -eq 1 ]; then
        shot=$((shot + 1))
        "$ANDROID_DIR/scripts/screenshot.sh" "demo-$(printf %02d $shot)" >/dev/null || true
    fi
    if [ -n "$FAKE_PID" ] && ! kill -0 "$FAKE_PID" 2>/dev/null; then break; fi
    [ -z "$FAKE_PID" ] && [ "$i" -ge 60 ] && break
done
"$ANDROID_DIR/scripts/screenshot.sh" demo-final >/dev/null || true
adb_emu logcat -d -s ROTOS > "$OUT_DIR/app_logcat.txt" || true
if [ -n "$FAKE_PID" ]; then
    wait "$FAKE_PID" && rc=0 || rc=$?
    tail -n 12 "$LOG"
    exit $rc
fi
