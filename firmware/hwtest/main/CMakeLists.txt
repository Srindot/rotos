# Test code + the real panel path: ESP32 HAL (display, input, time, log),
# ILI9341 init, 4-bit canvas, font, and the band presenter.
get_filename_component(ROOT ${CMAKE_CURRENT_LIST_DIR}/../../.. ABSOLUTE)
set(HAL ${ROOT}/firmware/hal/esp)
set(UI ${ROOT}/ui/firmware/src)

idf_component_register(
    SRCS hwtest_main.c ht_palette.c ht_buttons.c ht_screen.c ht_panel.c
         ${HAL}/hal_display_esp.c ${HAL}/hal_input_esp.c ${HAL}/hal_time_esp.c ${HAL}/hal_log_esp.c
         ${ROOT}/firmware/app/src/device/rt_ili9341.c
         ${UI}/draw/rt_canvas.c ${UI}/draw/rt_font.c ${UI}/theme/rt_theme.c ${UI}/views/rt_present.c
         ${ROOT}/core/src/base/rn_assert.c
    INCLUDE_DIRS . ${ROOT}/firmware/main ${ROOT}/firmware/hal/include ${ROOT}/firmware/app/include
                 ${ROOT}/ui/firmware/include ${ROOT}/core/include
    REQUIRES driver esp_adc esp_timer spi_flash log freertos)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wshadow -Werror -Wno-unused-parameter)
