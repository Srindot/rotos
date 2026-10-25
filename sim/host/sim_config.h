/*
 * sim_config.h - simulator-only configuration of the host HAL, set by
 * main() before any hal_*_init() call. Not part of the firmware.
 */
#ifndef SIM_CONFIG_H
#define SIM_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SIM_LINK_NONE = 0,     /* no phone                                    */
    SIM_LINK_FEED,         /* replay a .rfeed capture (virtual phone)     */
    SIM_LINK_TCP           /* live phone: Android app / virtual phone     */
} SimLinkMode;

#define SIM_MAX_PRESSES 16u

typedef struct {
    char button;           /* 'A' 'B' 'C'                                 */
    uint32_t at_ms;
    uint32_t hold_ms;
} SimPress;

typedef struct {
    bool headless;               /* no screen twin; virtual clock          */
    uint16_t screen_port;
    SimLinkMode link_mode;
    const char *feed_path;
    uint32_t feed_start_ms;      /* when the phone "connects"             */
    uint16_t listen_port;
    const char *state_dir;       /* "flash": map pack + settings files    */
    uint32_t map_capacity;       /* bytes, like the ESP32 partition       */
    float speed;                 /* real-time multiplier                  */
    SimPress presses[SIM_MAX_PRESSES];
    uint32_t n_presses;
} SimConfig;

void sim_configure(const SimConfig *c);
const SimConfig *sim_config(void);

/* Virtual clock (headless): main advances it one frame at a time. */
void sim_time_advance(uint32_t ms);

/* Display statistics and the twin's back channel. */
uint64_t sim_display_bytes(void);
bool sim_display_ok(void);
void sim_display_poll(void);
uint8_t sim_display_buttons(void);
int32_t sim_display_light(void);

/* Link statistics. */
bool sim_link_feed_done(void);
uint32_t sim_link_feed_end_ms(void);

#endif /* SIM_CONFIG_H */
