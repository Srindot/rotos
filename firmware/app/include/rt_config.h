/*
 * rt_config.h - compile-time configuration of the ROTOS application.
 */
#ifndef RT_CONFIG_H
#define RT_CONFIG_H

#define RT_VERSION_TEXT     "V3.0 BETA  SPEED 400"

#define RT_SCREEN_W         240
#define RT_SCREEN_H         320
#define RT_BAND_H           16                       /* rows per SPI transfer */
#define RT_BANDS            (RT_SCREEN_H / RT_BAND_H)
#define RT_FRAME_MS         33u                      /* ~30 fps target        */

#define RT_BOOT_MS          2000u
#define RT_PANEL_REFRESH_MS 2000u                    /* re-send panel setup (supply dips) */
#define RT_POWER_SETTLE_MS  300u                     /* bike USB rail settling before panel init */
#define RT_LINK_TIMEOUT_MS  3500u                    /* no packets: link lost */
#define RT_NAV_TIMEOUT_MS   3500u                    /* no NAV_STATE: stale   */
#define RT_RIDE_ABANDON_MS  90000u                   /* no NAV_STATE: ride over */
#define RT_MAPRX_STALL_MS   30000u                   /* no map chunk: give up */
#define RT_SUMMARY_MS       15000u                   /* ride summary card     */
#define RT_PREP_TIMEOUT_MS  10000u                   /* phone progress panel  */
#define RT_GUIDE_TIMEOUT_MS 8000u                    /* Google Maps mode      */
#define RT_THEME_HOLD_MS    3000u                    /* night flag must hold  */
#define RT_THEME_FADE_MS    1000u
#define RT_MOVING_MPS       1.5f                     /* slower = stopped      */
#define RT_ARRIVE_M         8.0f
#define RT_FLYOVER_CHIP_M   600.0f
#define RT_PING_MS          2000u
#define RT_PAGE_RETURN_MS   8000u                    /* riding: back to the map */
#define RT_PAGE_RETURN_MPS  3.0f
#define RT_LONG_PRESS_MS    800u
#define RT_DEBOUNCE_MS      30u

#define RT_GLASS_DEFAULT    70                       /* % toward panel colour */
#define RT_ANCHOR_X         120                      /* rider on screen       */
#define RT_ANCHOR_Y         212
#define RT_ANCHOR_Y_NORTH   168

#define RT_MAP_ACK_EVERY    8u                       /* chunks per ACK        */
#define RT_MAP_INFLATE_STEPS 4096u                   /* 512 B outputs per drain (bounds the deflate 1032:1 worst case) */

#endif /* RT_CONFIG_H */
