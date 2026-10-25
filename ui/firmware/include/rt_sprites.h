/*
 * rt_sprites.h - 16x16 sprite data (maneuvers, weather, mascot).
 */
#ifndef RT_SPRITES_H
#define RT_SPRITES_H

#include <stdbool.h>
#include <stdint.h>

#define RT_SPRITE 16

#define RT_MOOD_IDLE  0u
#define RT_MOOD_BLINK 1u
#define RT_MOOD_HAPPY 2u

/* Sprite for a maneuver type (rn_maneuver.h values); *mirror = draw flipped. */
const char *const *rt_sprite_maneuver(uint8_t type, bool *mirror);
/* Sprite for a WMO weather code. */
const char *const *rt_sprite_weather(uint8_t wmo, bool is_day);
const char *const *rt_sprite_mascot(uint8_t mood);
/* Telephone handset for the call popup. */
const char *const *rt_sprite_phone(void);

#endif /* RT_SPRITES_H */
