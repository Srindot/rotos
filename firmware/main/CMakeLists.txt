# One component: entry point + ESP32 HAL + portable app (logic, UI, core).
include(${CMAKE_CURRENT_LIST_DIR}/../sources.cmake)
file(GLOB HAL_SRCS ${RT_ROOT}/firmware/hal/esp/*.c)

idf_component_register(
    SRCS main.c ${HAL_SRCS} ${RT_APP_SOURCES}
    INCLUDE_DIRS . ${RT_APP_INCLUDE_DIRS}
    REQUIRES driver esp_partition nvs_flash bt esp_timer esp_adc log freertos)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wshadow -Werror -Wno-unused-parameter)
