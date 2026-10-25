#!/usr/bin/env bash
# Hardware test: build, flash and open the serial monitor (Docker, ESP-IDF 5.3).
#   firmware/scripts/hwtest.sh                         ESP32 DevKit V1, first USB serial port found
#   firmware/scripts/hwtest.sh esp32 /dev/ttyUSB1      another port
#   firmware/scripts/hwtest.sh esp32c3 /dev/ttyACM0    XIAO ESP32-C3
#   firmware/scripts/hwtest.sh esp32 build             build only, no board needed
# Press Ctrl+] to leave the monitor.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TARGET="${1:-esp32}"
# First USB serial port found: CH9102 / native USB show as ttyACM, CP2102 / CH340 as ttyUSB.
DEFAULT_PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1 || true)
PORT="${2:-$DEFAULT_PORT}"
IDF_IMAGE="espressif/idf:v5.3.1"
B="/project/build/hwtest-$TARGET"
IDF="idf.py -B $B -DSDKCONFIG=$B/sdkconfig"

docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT:/project" -w /project/firmware/hwtest "$IDF_IMAGE" \
    bash -c "$IDF set-target $TARGET >/dev/null && $IDF build"
[ "$PORT" = build ] && exit 0

if [ -z "$PORT" ] || [ ! -e "$PORT" ]; then
    echo "No board on $PORT. Plug it in and check: ls /dev/ttyUSB* /dev/ttyACM*" >&2
    exit 1
fi
docker run --rm -it --device "$PORT" -u "$(id -u):$(id -g)" --group-add dialout -e HOME=/tmp \
    -v "$ROOT:/project" -w /project/firmware/hwtest "$IDF_IMAGE" \
    $IDF -p "$PORT" flash monitor
