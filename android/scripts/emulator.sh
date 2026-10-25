#!/usr/bin/env bash
# Start the Android emulator in Docker (detached) and wait until it has booted.
#   scripts/emulator.sh             window on the host X11 display ($DISPLAY, default :0)
#   scripts/emulator.sh --headless  no window (CI-style checks, screenshots via adb)
#   scripts/emulator.sh --stop      stop and remove the emulator container
#   EMU_API=35 scripts/emulator.sh  Android 15 instead of Android 14
# Uses --network host so the emulator's 10.0.2.2 is the host's 127.0.0.1
# (fake_rotos.py / the simulator on port 7400), and /dev/kvm for speed.
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [ "${1:-}" = "--stop" ]; then
    docker rm -f "$EMU_NAME" >/dev/null 2>&1 || true
    echo "emulator stopped"
    exit 0
fi
[ -r /dev/kvm ] && [ -w /dev/kvm ] || { echo "no access to /dev/kvm (add yourself to group kvm)"; exit 1; }
ensure_image "$EMU_IMAGE" "$EMU_TARGET"
docker rm -f "$EMU_NAME" >/dev/null 2>&1 || true

EMU_ARGS=(-avd "$EMU_AVD" -no-snapshot -no-audio -no-boot-anim -gpu swiftshader_indirect -no-metrics
          -timezone "${EMU_TZ:-Asia/Kolkata}")   # Open-Meteo answers in local time
DOCKER_ARGS=(-d --name "$EMU_NAME" --device /dev/kvm --network host
             -v "$REPO_DIR:/work")
if [ "${1:-}" = "--headless" ]; then
    EMU_ARGS+=(-no-window)
else
    export DISPLAY=${DISPLAY:-:0}
    xhost +SI:localuser:root >/dev/null 2>&1 || xhost +local: >/dev/null 2>&1 || true
    DOCKER_ARGS+=(-e DISPLAY="$DISPLAY" -v /tmp/.X11-unix:/tmp/.X11-unix -e QT_X11_NO_MITSHM=1)
fi
docker run "${DOCKER_ARGS[@]}" "$EMU_IMAGE" emulator "${EMU_ARGS[@]}" >/dev/null
echo "emulator container $EMU_NAME started; waiting for boot..."

for i in $(seq 1 180); do
    if [ "$(docker exec "$EMU_NAME" adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ]; then
        docker exec "$EMU_NAME" adb shell settings put global window_animation_scale 0 >/dev/null 2>&1 || true
        docker exec "$EMU_NAME" adb shell settings put global transition_animation_scale 0 >/dev/null 2>&1 || true
        docker exec "$EMU_NAME" adb shell input keyevent 82 >/dev/null 2>&1 || true
        echo "booted after $((i * 2)) s"
        exit 0
    fi
    if ! docker ps -q -f name="^${EMU_NAME}$" | grep -q .; then
        echo "emulator container exited:"; docker logs --tail 40 "$EMU_NAME"; exit 1
    fi
    sleep 2
done
echo "emulator did not boot in 6 min"; docker logs --tail 40 "$EMU_NAME"; exit 1
