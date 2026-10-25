# ROTOS: one entry point for every build, test and install (all Docker-based
# except the host C build). `make help` lists the targets.
.PHONY: help host test test-core test-android nasa sim sim-headless firmware flash hwtest \
        android release install-phone check

help:                 ## this list
	@grep -E '^[a-z-]+:.*## ' $(MAKEFILE_LIST) | awk -F':.*## ' '{printf "  make %-15s %s\n", $$1, $$2}'

host:                 ## core library, unit tests and simulator (build/host)
	cmake -S . -B build/host >/dev/null && cmake --build build/host -j

test-core: host       ## C unit tests (core)
	./build/host/core/rotos_nav_tests

nasa:                 ## NASA/JPL coding-standard check + cppcheck
	python3 tools/nasa_check.py

test-android:         ## Android JVM unit tests
	android/scripts/build.sh test

test: test-core nasa test-android  ## everything that must stay green

sim:                  ## simulated ride on the screen twin
	sim/run.sh

sim-headless:         ## simulated ride, frames to out/snaps
	sim/run.sh --headless

firmware:             ## ESP32 DevKit V1 firmware
	firmware/scripts/build.sh esp32

flash:                ## flash the DevKit and open the serial monitor
	firmware/scripts/flash.sh esp32

hwtest:               ## hardware test firmware: build, flash, monitor
	firmware/scripts/hwtest.sh esp32

android:              ## debug APK -> android/out/rotos-debug.apk
	android/scripts/build.sh

release:              ## signed release APK -> android/out/rotos-release.apk
	android/scripts/build.sh release

install-phone: release  ## install the release APK on the phone over USB
	android/scripts/install_phone.sh

check: test firmware  ## tests + firmware build
