#!/usr/bin/env bash
# Flash and open the serial monitor (USB cable to the board).
#   firmware/scripts/flash.sh [esp32|esp32c3|esp32s3] [/dev/ttyACM0]
#   default esp32 = ESP32 DevKit V1 (micro-B; port found automatically)
# Press Ctrl+] to leave the monitor. Build first with firmware/scripts/build.sh.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TARGET="${1:-esp32}"
# First USB serial port found: CH9102 / native USB show as ttyACM, CP2102 / CH340 as ttyUSB.
DEFAULT_PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1 || true)
PORT="${2:-$DEFAULT_PORT}"
if [ -z "$PORT" ] || [ ! -e "$PORT" ]; then
    echo "No board found. Plug it in and check: ls /dev/ttyUSB* /dev/ttyACM*" >&2
    exit 1
fi
docker run --rm -it --device "$PORT" -u "$(id -u):$(id -g)" --group-add dialout -e HOME=/tmp \
    -v "$ROOT:/project" -w /project/firmware espressif/idf:v5.3.1 \
    idf.py -B "/project/build/$TARGET" -DSDKCONFIG="/project/build/$TARGET/sdkconfig" -p "$PORT" flash monitor
