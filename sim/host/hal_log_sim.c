/*
 * hal_log_sim.c - log lines on stdout, stamped with the device clock.
 */
#include <stdio.h>

#include "hal_log.h"
#include "hal_time.h"

void hal_log(const char *tag, const char *msg, int32_t value)
{
    (void)printf("[%8.1f s] %-6s %s %d\n", (double)hal_time_ms() / 1000.0, (tag != NULL) ? tag : "",
                 (msg != NULL) ? msg : "", (int)value);
    (void)fflush(stdout);
}
