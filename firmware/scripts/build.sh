#!/usr/bin/env bash
# Build the ROTOS firmware with ESP-IDF in Docker (nothing to install locally).
#   firmware/scripts/build.sh            ESP32 DevKit V1   -> build/esp32/rotos_firmware.bin
#   firmware/scripts/build.sh esp32c3    ESP32-C3 (XIAO)   -> build/esp32c3/rotos_firmware.bin
#   firmware/scripts/build.sh esp32s3    ESP32-S3 DevKit   -> build/esp32s3/rotos_firmware.bin
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TARGET="${1:-esp32}"
IDF_IMAGE="espressif/idf:v5.3.1"
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$ROOT:/project" -w /project/firmware "$IDF_IMAGE" \
    bash -c "idf.py -B /project/build/$TARGET -DSDKCONFIG=/project/build/$TARGET/sdkconfig set-target $TARGET >/dev/null &&
             idf.py -B /project/build/$TARGET -DSDKCONFIG=/project/build/$TARGET/sdkconfig build"
