# ROTOS firmware - ESP-IDF project (ESP32 DevKit V1; ESP32-C3/S3 also build).
#   firmware/scripts/build.sh esp32      (Docker, nothing to install)
#
# Sources: main/ (entry point), hal/esp/ (this hardware), app/ (portable logic),
# ../ui/firmware (screens) and ../core (navigation library); see sources.cmake.
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rotos_firmware)
