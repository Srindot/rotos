# ROTOS hardware test - a small ESP-IDF project that checks the wiring of the
# TFT and the three buttons with the same drivers as the real firmware.
#
#   firmware/scripts/hwtest.sh           build, flash and open the serial monitor
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rotos_hwtest)
