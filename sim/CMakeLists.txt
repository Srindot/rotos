# ROTOS simulator: the firmware application on the desktop.
#
#   rotos_screen   ILI9341 screen twin (SDL2), independent of ROTOS
#   rotos_sim      firmware/app + ui/firmware + core + host HAL (sim/host)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

include(${CMAKE_CURRENT_LIST_DIR}/../firmware/sources.cmake)

set(SIM_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
                 -Wdouble-promotion -Wstrict-prototypes -Wmissing-prototypes -Werror)

# The portable firmware application, exactly as it is compiled for the ESP32.
add_library(rotos_app STATIC ${RT_APP_SOURCES})
target_include_directories(rotos_app PUBLIC ${RT_APP_INCLUDE_DIRS})
target_compile_features(rotos_app PUBLIC c_std_11)
target_compile_options(rotos_app PRIVATE ${SIM_WARNINGS})
target_link_libraries(rotos_app PUBLIC m)

file(GLOB SIM_HOST_SOURCES ${CMAKE_CURRENT_LIST_DIR}/host/*.c)
add_executable(rotos_sim ${SIM_HOST_SOURCES})
target_link_libraries(rotos_sim PRIVATE rotos_app)
target_compile_options(rotos_sim PRIVATE -Wall -Wextra -Wshadow -Werror)

file(GLOB SIM_SCREEN_SOURCES ${CMAKE_CURRENT_LIST_DIR}/screen/*.c)
add_executable(rotos_screen ${SIM_SCREEN_SOURCES})
target_include_directories(rotos_screen PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(rotos_screen PRIVATE ${SDL2_LIBRARIES})
target_compile_options(rotos_screen PRIVATE -Wall -Wextra -Wshadow -Werror)
