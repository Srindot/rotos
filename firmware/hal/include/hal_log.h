/*
 * hal_log.h - minimal logging: a tag, a message and one number. No format
 * strings (JPL rule 8 spirit: no variadic interfaces).
 */
#ifndef HAL_LOG_H
#define HAL_LOG_H

#include <stdint.h>

void hal_log(const char *tag, const char *msg, int32_t value);

#endif /* HAL_LOG_H */
