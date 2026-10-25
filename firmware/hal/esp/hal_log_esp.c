/*
 * hal_log_esp.c - ESP-IDF logging (USB-Serial-JTAG console).
 */
#include <stddef.h>

#include "esp_log.h"
#include "hal_log.h"

void hal_log(const char *tag, const char *msg, int32_t value)
{
    ESP_LOGI((tag != NULL) ? tag : "rotos", "%s %ld", (msg != NULL) ? msg : "", (long)value);
}
