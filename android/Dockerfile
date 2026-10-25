# ROTOS Android toolchain.
#
#   build     JDK 17 + Android SDK 34 + NDK 26 + CMake 3.22 + Gradle 8.7
#   emulator  build + Android emulator + Google APIs x86_64 system image + AVD
#
# The source is never copied in: run with the repo mounted (see scripts/).
#   docker build --target build    -t rotos-apk-build android/
#   docker build --target emulator -t rotos-apk-emu   android/
FROM eclipse-temurin:17-jdk-jammy AS build

ARG CMDLINE_TOOLS=11076708
ARG GRADLE_VERSION=8.7
ARG NDK_VERSION=26.3.11579264

ENV ANDROID_HOME=/opt/android-sdk \
    ANDROID_SDK_ROOT=/opt/android-sdk \
    GRADLE_HOME=/opt/gradle \
    GRADLE_USER_HOME=/root/.gradle
ENV PATH=${PATH}:/opt/gradle/bin:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools

RUN apt-get update \
 && apt-get install -y --no-install-recommends unzip wget ca-certificates python3 \
 && rm -rf /var/lib/apt/lists/*

RUN wget -q https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip -O /tmp/g.zip \
 && unzip -q /tmp/g.zip -d /opt && mv /opt/gradle-${GRADLE_VERSION} /opt/gradle && rm /tmp/g.zip

RUN mkdir -p ${ANDROID_HOME}/cmdline-tools \
 && wget -q https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS}_latest.zip -O /tmp/c.zip \
 && unzip -q /tmp/c.zip -d ${ANDROID_HOME}/cmdline-tools \
 && mv ${ANDROID_HOME}/cmdline-tools/cmdline-tools ${ANDROID_HOME}/cmdline-tools/latest && rm /tmp/c.zip

RUN yes | sdkmanager --licenses > /dev/null \
 && sdkmanager --install "platform-tools" "platforms;android-34" "build-tools;34.0.0" \
      "ndk;${NDK_VERSION}" "cmake;3.22.1" > /dev/null

WORKDIR /work/android
CMD ["gradle", "--no-daemon", "assembleDebug"]

# ---------------------------------------------------------------------------
FROM build AS emulator

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      libpulse0 libgl1 libegl1 libnss3 libxcomposite1 libxcursor1 libxi6 libxtst6 \
      libasound2 libxdamage1 libxrandr2 libxkbcommon-x11-0 libxkbfile1 libx11-xcb1 \
      libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
      libxcb-render-util0 libxcb-shape0 libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 \
      libdbus-1-3 libfontconfig1 libgbm1 libdrm2 libtcmalloc-minimal4 libsm6 libice6 \
 && rm -rf /var/lib/apt/lists/*

RUN sdkmanager --install "emulator" "system-images;android-34;google_apis;x86_64" > /dev/null \
 && echo no | avdmanager create avd -n rotos -k "system-images;android-34;google_apis;x86_64" -d pixel_6 \
 && printf 'hw.keyboard=yes\nhw.gpu.enabled=yes\nhw.ramSize=3072\ndisk.dataPartition.size=4G\n' \
      >> /root/.android/avd/rotos.avd/config.ini

ENV PATH=${PATH}:/opt/android-sdk/emulator
CMD ["emulator", "-avd", "rotos", "-no-snapshot", "-no-audio", "-no-boot-anim"]

# ---------------------------------------------------------------------------
# Android 15 (API 35), e.g. Motorola Razr 60. Select with EMU_API=35.
FROM emulator AS emulator35

RUN sdkmanager --install "system-images;android-35;google_apis;x86_64" > /dev/null \
 && echo no | avdmanager create avd -n rotos35 -k "system-images;android-35;google_apis;x86_64" -d pixel_6 \
 && printf 'hw.keyboard=yes\nhw.gpu.enabled=yes\nhw.ramSize=3072\ndisk.dataPartition.size=4G\n' \
      >> /root/.android/avd/rotos35.avd/config.ini
