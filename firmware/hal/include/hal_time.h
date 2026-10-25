/*
 * hal_time.h - monotonic milliseconds and frame pacing.
 */
#ifndef HAL_TIME_H
#define HAL_TIME_H

#include <stdint.h>

/* Milliseconds since boot (wraps after ~49 days; use unsigned differences). */
uint32_t hal_time_ms(void);
/* Sleep until hal_time_ms() reaches deadline_ms (returns at once if past). */
void hal_time_sleep_until(uint32_t deadline_ms);

#endif /* HAL_TIME_H */
