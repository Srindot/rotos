/*
 * hal_display_esp.c - ILI9341 on SPI2 with DMA, D/C and RST as GPIOs,
 * backlight PWM on LEDC.
 *
 * Transfers up to 32 bytes (commands, parameters) are polled and finish
 * before the call returns, so callers may pass stack buffers. Larger ones
 * (16-row pixel bands) are queued for DMA; at most one is in flight and it
 * is awaited before the next transfer starts (see hal_display.h).
 *
 * Note: the SPI driver and LEDC are vendor APIs; this file is the only place
 * that touches them.
 */
#include <string.h>

#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_display.h"
#include "hal_log.h"

#define SMALL_XFER 32u

static spi_device_handle_t s_dev;
static spi_transaction_t s_trans;
static bool s_inflight;

static void wait_inflight(void)
{
    if (s_inflight) {
        spi_transaction_t *done = NULL;
        (void)spi_device_get_trans_result(s_dev, &done, portMAX_DELAY);
        s_inflight = false;
    }
}

static bool init_pins(void)
{
    gpio_config_t io;
    (void)memset(&io, 0, sizeof io);
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = 1ULL << BOARD_PIN_DC;
    if (BOARD_PIN_RST >= 0) {
        io.pin_bit_mask |= 1ULL << BOARD_PIN_RST;
    }
    return gpio_config(&io) == ESP_OK;
}

static bool init_backlight(void)
{
    if (BOARD_PIN_BL < 0) {
        return true;                                /* wired to 3V3: always on */
    }
    ledc_timer_config_t tc;
    (void)memset(&tc, 0, sizeof tc);
    tc.speed_mode = LEDC_LOW_SPEED_MODE;
    tc.duty_resolution = LEDC_TIMER_10_BIT;
    tc.timer_num = LEDC_TIMER_0;
    tc.freq_hz = 5000;
    tc.clk_cfg = LEDC_AUTO_CLK;
    ledc_channel_config_t cc;
    (void)memset(&cc, 0, sizeof cc);
    cc.gpio_num = BOARD_PIN_BL;
    cc.speed_mode = LEDC_LOW_SPEED_MODE;
    cc.channel = LEDC_CHANNEL_0;
    cc.timer_sel = LEDC_TIMER_0;
    cc.duty = 1023;
    return ledc_timer_config(&tc) == ESP_OK && ledc_channel_config(&cc) == ESP_OK;
}

uint8_t hal_display_madctl(void)
{
    return (uint8_t)BOARD_PANEL_MADCTL;
}

bool hal_display_init(void)
{
    spi_bus_config_t bus;
    (void)memset(&bus, 0, sizeof bus);
    bus.mosi_io_num = BOARD_PIN_MOSI;
    bus.miso_io_num = -1;
    bus.sclk_io_num = BOARD_PIN_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = BOARD_SPI_MAX_XFER;
    spi_device_interface_config_t dev;
    (void)memset(&dev, 0, sizeof dev);
    dev.clock_speed_hz = BOARD_SPI_HZ;
    dev.mode = 0;
    dev.spics_io_num = BOARD_PIN_CS;
    dev.queue_size = 2;
    bool ok = init_pins() && init_backlight();
    ok = ok && spi_bus_initialize(BOARD_SPI_HOST, &bus, SPI_DMA_CH_AUTO) == ESP_OK;
    ok = ok && spi_bus_add_device(BOARD_SPI_HOST, &dev, &s_dev) == ESP_OK;
    hal_log("lcd", ok ? "SPI ready, Hz" : "SPI init failed", BOARD_SPI_HZ);
    return ok;
}

void hal_display_reset(void)
{
    if (BOARD_PIN_RST < 0) {
        return;
    }
    (void)gpio_set_level(BOARD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    (void)gpio_set_level(BOARD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void polled(const uint8_t *bytes, uint32_t n)
{
    spi_transaction_t t;
    (void)memset(&t, 0, sizeof t);
    t.length = n * 8u;
    t.tx_buffer = bytes;
    (void)spi_device_polling_transmit(s_dev, &t);
}

void hal_display_command(uint8_t cmd)
{
    wait_inflight();
    (void)gpio_set_level(BOARD_PIN_DC, 0);
    polled(&cmd, 1u);
    (void)gpio_set_level(BOARD_PIN_DC, 1);
}

void hal_display_data(const uint8_t *bytes, uint32_t n)
{
    if (bytes == NULL || n == 0u || n > BOARD_SPI_MAX_XFER) {
        return;
    }
    wait_inflight();
    (void)gpio_set_level(BOARD_PIN_DC, 1);
    if (n <= SMALL_XFER) {
        polled(bytes, n);
        return;
    }
    (void)memset(&s_trans, 0, sizeof s_trans);
    s_trans.length = n * 8u;
    s_trans.tx_buffer = bytes;
    s_inflight = spi_device_queue_trans(s_dev, &s_trans, portMAX_DELAY) == ESP_OK;
}

void hal_display_flush(void)
{
    wait_inflight();
}

void hal_display_backlight(uint8_t pct)
{
    if (BOARD_PIN_BL < 0) {
        return;
    }
    uint32_t duty = ((uint32_t)((pct > 100u) ? 100u : pct) * 1023u) / 100u;
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
