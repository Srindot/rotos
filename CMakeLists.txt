# ROTOS - host build (core navigation library, tests, simulator).
# The ESP32 firmware is built with ESP-IDF (firmware/scripts/build.sh), the
# Android app with Gradle (android/scripts/build.sh); `make help` lists all.
cmake_minimum_required(VERSION 3.16)
project(rotos C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(ROTOS_BUILD_TESTS ON)
enable_testing()
add_subdirectory(core)
add_subdirectory(sim)
