#!/usr/bin/env bash
# Install android/out/rotos-debug.apk into the running emulator (all runtime
# permissions granted) and launch it.
set -euo pipefail
source "$(dirname "$0")/common.sh"
[ -f "$OUT_DIR/rotos-debug.apk" ] || "$ANDROID_DIR/scripts/build.sh"
if ! adb_emu install -r -g /work/android/out/rotos-debug.apk; then
    echo "update refused (signing key changed?): reinstalling"
    adb_emu uninstall dev.rotos.app || true
    adb_emu install -g /work/android/out/rotos-debug.apk
fi
adb_emu shell am start -n dev.rotos.app/.ui.MainActivity "$@"
