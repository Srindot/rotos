# Shared settings for the android/scripts/*.sh helpers (sourced, not run).
ANDROID_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
REPO_DIR=$(cd "$ANDROID_DIR/.." && pwd)
OUT_DIR="$ANDROID_DIR/out"
BUILD_IMAGE=rotos-apk-build
EMU_API=${EMU_API:-34}          # 34 = Android 14, 35 = Android 15
if [ "$EMU_API" = "35" ]; then
    EMU_IMAGE=rotos-apk-emu35; EMU_TARGET=emulator35; EMU_AVD=rotos35
else
    EMU_IMAGE=rotos-apk-emu;   EMU_TARGET=emulator;   EMU_AVD=rotos
fi
EMU_NAME=rotos-emu
GRADLE_VOLUME=rotos-gradle
ANDROID_VOLUME=rotos-android   # keeps the debug signing key stable across builds
ADB_PORT=5555          # emulator console 5554, adb 5555 (host network)
mkdir -p "$OUT_DIR"

ensure_image() {   # ensure_image <tag> <target>
    docker image inspect "$1" >/dev/null 2>&1 || docker build --target "$2" -t "$1" "$ANDROID_DIR"
}

# Run adb inside the running emulator container.
adb_emu() {
    docker exec "$EMU_NAME" adb "$@"
}
