#!/usr/bin/env bash
# Install ROTOS on a real phone over USB (USB debugging on): grant every
# permission, exempt it from battery optimisation, launch it.
#   android/scripts/install_phone.sh            release build (android/out/rotos-release.apk)
#   android/scripts/install_phone.sh debug      debug build
# Debug and release are signed with different keys: switching between them
# uninstalls the other first (recent destinations are lost, nothing else).
# adb runs in the build image; its key lives in the rotos-android volume, so
# the phone's "Always allow from this computer" is remembered.
set -euo pipefail
source "$(dirname "$0")/common.sh"
KIND="${1:-release}"
[ -f "$OUT_DIR/rotos-$KIND.apk" ] || "$ANDROID_DIR/scripts/build.sh" "$([ "$KIND" = release ] && echo release)"
PERMS="ACCESS_FINE_LOCATION ACCESS_COARSE_LOCATION BLUETOOTH_SCAN BLUETOOTH_CONNECT POST_NOTIFICATIONS
       READ_PHONE_STATE READ_CALL_LOG READ_CONTACTS ANSWER_PHONE_CALLS SEND_SMS"
docker run --rm --privileged -v /dev/bus/usb:/dev/bus/usb -v rotos-android:/root/.android \
    -v "$OUT_DIR:/out:ro" rotos-apk-build bash -c "
adb start-server >/dev/null 2>&1
adb wait-for-device
if ! adb install -r -g /out/rotos-$KIND.apk 2>&1 | tee /tmp/install.log | grep -q Success; then
    if grep -q UPDATE_INCOMPATIBLE /tmp/install.log; then
        echo 'signed with another key: uninstalling the old build first'
        adb uninstall dev.rotos.app >/dev/null
        adb install -g /out/rotos-$KIND.apk
    else
        cat /tmp/install.log; exit 1
    fi
fi
for p in $(echo $PERMS); do adb shell pm grant dev.rotos.app android.permission.\$p || true; done
adb shell cmd deviceidle whitelist +dev.rotos.app
adb shell cmd appops set dev.rotos.app RUN_ANY_IN_BACKGROUND allow
adb shell cmd notification allow_listener dev.rotos.app/dev.rotos.app.mirror.MapsNotificationListener || true
adb shell am start -n dev.rotos.app/.ui.MainActivity >/dev/null
echo 'ROTOS installed and started'"
