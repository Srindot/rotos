/*
 * hal_input_esp.c - three buttons to GND (internal pull-ups, pressed = 0)
 * and an optional LDR divider on an ADC pin (LDR to 3V3, 10 k to GND:
 * brighter = higher voltage).
 */
#include <string.h>

#include "board.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "hal_input.h"
#include "hal_time.h"

#define LIGHT_PERIOD_MS 100u

static const int BUTTON_PINS[HAL_BUTTON_COUNT] = {BOARD_PIN_BTN_A, BOARD_PIN_BTN_B, BOARD_PIN_BTN_C};
static adc_oneshot_unit_handle_t s_adc;
static bool s_have_adc;
static int32_t s_light = -1;
static uint32_t s_light_ms;

static bool init_adc(void)
{
    if (BOARD_PIN_LDR < 0) {
        return true;
    }
    adc_oneshot_unit_init_cfg_t unit;
    (void)memset(&unit, 0, sizeof unit);
    unit.unit_id = ADC_UNIT_1;
    adc_oneshot_chan_cfg_t ch;
    (void)memset(&ch, 0, sizeof ch);
    ch.atten = ADC_ATTEN_DB_12;
    ch.bitwidth = ADC_BITWIDTH_12;
    s_have_adc = adc_oneshot_new_unit(&unit, &s_adc) == ESP_OK &&
                 adc_oneshot_config_channel(s_adc, (adc_channel_t)BOARD_LDR_CHANNEL, &ch) == ESP_OK;
    return s_have_adc;
}

bool hal_input_init(void)
{
    gpio_config_t io;
    (void)memset(&io, 0, sizeof io);
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    for (uint32_t i = 0u; i < HAL_BUTTON_COUNT; i++) {
        io.pin_bit_mask |= 1ULL << BUTTON_PINS[i];
    }
    bool ok = gpio_config(&io) == ESP_OK;
    return init_adc() && ok;
}

uint8_t hal_input_buttons(void)
{
    uint8_t mask = 0u;
    for (uint32_t i = 0u; i < HAL_BUTTON_COUNT; i++) {
        if (gpio_get_level(BUTTON_PINS[i]) == 0) {
            mask |= (uint8_t)(1u << i);
        }
    }
    return mask;
}

int32_t hal_input_light(void)
{
    if (!s_have_adc) {
        return -1;
    }
    uint32_t now = hal_time_ms();
    if (s_light >= 0 && now - s_light_ms < LIGHT_PERIOD_MS) {
        return s_light;
    }
    int raw = 0;
    if (adc_oneshot_read(s_adc, (adc_channel_t)BOARD_LDR_CHANNEL, &raw) != ESP_OK) {
        return s_light;
    }
    s_light = (int32_t)((raw < 0) ? 0 : ((raw > 4095) ? 4095 : raw)) * 100 / 4095;
    s_light_ms = now;
    return s_light;
}
