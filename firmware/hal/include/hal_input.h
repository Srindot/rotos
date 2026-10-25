/*
 * hal_input.h - buttons and the ambient light sensor.
 *
 * Buttons are sampled, not interrupt-driven: rt_input debounces them.
 */
#ifndef HAL_INPUT_H
#define HAL_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#define HAL_BUTTON_COUNT 3u
#define HAL_BUTTON_A     0x01u   /* view: dashboard <-> map       */
#define HAL_BUTTON_B     0x02u   /* theme: auto / day / night      */
#define HAL_BUTTON_C     0x04u   /* reserved: calls (future)       */

bool hal_input_init(void);
/* Bit mask of buttons currently held down. */
uint8_t hal_input_buttons(void);
/* Ambient light 0..100 (% of full scale), or -1 without a sensor. */
int32_t hal_input_light(void);

#endif /* HAL_INPUT_H */
