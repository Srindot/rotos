cmake_minimum_required(VERSION 3.16)
project(rotos CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ── ROTOS core: portable, no OS / no SDL. This is what goes on the ESP32. ──
add_library(rotos_core STATIC
  core/ili9341.cpp core/canvas.cpp core/theme.cpp core/font.cpp core/icons.cpp
  core/protocol.cpp core/mappack.cpp core/tracker.cpp core/map_view.cpp
  core/hud.cpp core/rotos.cpp)
target_include_directories(rotos_core PUBLIC core)
target_compile_options(rotos_core PRIVATE -Wall -Wextra)

# ── Screen twin: standalone ILI9341 emulator, knows nothing about ROTOS. ──
find_package(SDL2 REQUIRED)
add_executable(rotos_screen screen/main.cpp)
target_include_directories(rotos_screen PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(rotos_screen PRIVATE ${SDL2_LIBRARIES})

# ── Desktop host: runs the core, plays the phone, drives the screen. ──
add_executable(rotos_sim host/main.cpp host/tcp_spi_bus.cpp)
target_link_libraries(rotos_sim PRIVATE rotos_core)
target_compile_options(rotos_sim PRIVATE -Wall -Wextra)
