/*
 * hal_time_esp.c - esp_timer milliseconds and FreeRTOS frame pacing.
 */
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_time.h"

uint32_t hal_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void hal_time_sleep_until(uint32_t deadline_ms)
{
    int32_t left = (int32_t)(deadline_ms - hal_time_ms());
    /* Always yield at least one tick so the idle task (watchdog) runs. */
    TickType_t ticks = (left > 0) ? pdMS_TO_TICKS((uint32_t)left) : 1;
    vTaskDelay((ticks > 0) ? ticks : 1);
}
