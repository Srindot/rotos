#!/usr/bin/env bash
# Build the APK in Docker.
#   scripts/build.sh                  debug   -> android/out/rotos-debug.apk
#   scripts/build.sh release          release -> android/out/rotos-release.apk (R8, signed)
#   scripts/build.sh test             JVM unit tests
#   scripts/build.sh <gradle tasks>   anything else, e.g. clean assembleDebug
# Google key: android/local.properties MAPS_API_KEY=... or env MAPS_API_KEY.
# The release key is created on first use in android/keystore/ (gitignored, back it
# up: a different key means uninstalling before the next update).
set -euo pipefail
source "$(dirname "$0")/common.sh"
ensure_image "$BUILD_IMAGE" build

make_release_key() {
    [ -f "$ANDROID_DIR/keystore.properties" ] && return 0
    mkdir -p "$ANDROID_DIR/keystore"
    local pw
    pw=$(head -c 24 /dev/urandom | base64 | tr -dc 'A-Za-z0-9' | head -c 24)
    docker run --rm -v "$ANDROID_DIR/keystore:/ks" "$BUILD_IMAGE" \
        keytool -genkeypair -keystore /ks/rotos-release.jks -alias rotos -keyalg RSA -keysize 4096 \
        -validity 10000 -storepass "$pw" -keypass "$pw" -dname "CN=ROTOS, O=Personal" >/dev/null 2>&1
    docker run --rm -v "$ANDROID_DIR/keystore:/ks" "$BUILD_IMAGE" chown "$(id -u):$(id -g)" /ks/rotos-release.jks
    printf 'storeFile=keystore/rotos-release.jks\nstorePassword=%s\nkeyAlias=rotos\nkeyPassword=%s\n' "$pw" "$pw" \
        > "$ANDROID_DIR/keystore.properties"
    chmod 600 "$ANDROID_DIR/keystore.properties"
    echo "release key created: android/keystore/rotos-release.jks (back it up)"
}

case "${1:-}" in
    "") TASKS="assembleDebug" ;;
    release) make_release_key; TASKS="assembleRelease" ;;
    test) TASKS="testDebugUnitTest" ;;
    *) TASKS="$*" ;;
esac

docker run --rm \
    -v "$REPO_DIR:/work" -v "$GRADLE_VOLUME:/root/.gradle" -v "$ANDROID_VOLUME:/root/.android" -w /work/android \
    -e MAPS_API_KEY="${MAPS_API_KEY:-}" \
    "$BUILD_IMAGE" bash -c "
        status=0
        gradle --no-daemon --console=plain $TASKS || status=\$?
        chown -R $(id -u):$(id -g) /work/android/app/build /work/android/.gradle /work/android/app/.cxx 2>/dev/null || true
        exit \$status"

# Gradle's own output folder is build/outputs/apk/ (the package type, not our folder name).
for kind in debug release; do
    APK="$ANDROID_DIR/app/build/outputs/apk/$kind/app-$kind.apk"
    if [ -f "$APK" ] && [ "$APK" -nt "$OUT_DIR/rotos-$kind.apk" ]; then
        cp "$APK" "$OUT_DIR/rotos-$kind.apk"
        echo "APK: $OUT_DIR/rotos-$kind.apk ($(du -h "$OUT_DIR/rotos-$kind.apk" | cut -f1))"
    fi
done
# An assemble that leaves no fresh APK behind is an error, not a silent stale copy.
case "$TASKS" in
    *assembleDebug*) [ ! "$ANDROID_DIR/app/build/outputs/apk/debug/app-debug.apk" -nt "$OUT_DIR/rotos-debug.apk" ] ||
        { echo "debug APK not copied to $OUT_DIR" >&2; exit 1; } ;;
esac
case "$TASKS" in
    *assembleRelease*) [ ! "$ANDROID_DIR/app/build/outputs/apk/release/app-release.apk" -nt "$OUT_DIR/rotos-release.apk" ] ||
        { echo "release APK not copied to $OUT_DIR" >&2; exit 1; } ;;
esac
