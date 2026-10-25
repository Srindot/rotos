# ROTOS portable firmware application: logic (firmware/app) + screens
# (ui/firmware) + the core navigation library. Included by the ESP-IDF build
# (firmware/main) and the desktop simulator (sim/), which compile it identically.
#   RT_APP_SOURCES       portable .c files (no ESP-IDF, no SDL)
#   RT_APP_INCLUDE_DIRS  their public headers, plus the HAL interface
get_filename_component(RT_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
include(${RT_ROOT}/core/sources.cmake)

file(GLOB_RECURSE RT_LOGIC_SOURCES ${RT_ROOT}/firmware/app/src/*.c)
file(GLOB_RECURSE RT_UI_SOURCES ${RT_ROOT}/ui/firmware/src/*.c)
set(RT_APP_SOURCES ${RT_LOGIC_SOURCES} ${RT_UI_SOURCES} ${RN_CORE_SOURCES})
set(RT_APP_INCLUDE_DIRS
    ${RT_ROOT}/firmware/app/include
    ${RT_ROOT}/ui/firmware/include
    ${RT_ROOT}/firmware/hal/include
    ${RN_INCLUDE_DIR})
