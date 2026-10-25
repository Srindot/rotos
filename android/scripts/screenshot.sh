#!/usr/bin/env bash
# scripts/screenshot.sh [name]  ->  android/out/<name>.png from the running emulator
set -euo pipefail
source "$(dirname "$0")/common.sh"
NAME=${1:-screen-$(date +%H%M%S)}
docker exec "$EMU_NAME" adb exec-out screencap -p > "$OUT_DIR/$NAME.png"
echo "$OUT_DIR/$NAME.png"
