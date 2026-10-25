/*
 * rt_colors.h - the 16 palette indices. Every pixel ROTOS draws is one of
 * these 4-bit indices (NES / Game Boy style): a theme maps them to RGB565,
 * so switching day/night recolours everything for free.
 */
#ifndef RT_COLORS_H
#define RT_COLORS_H

enum {
    /* 0..7: map classes, produced by the phone (navigation/rn_mapgen) */
    RT_C_LAND = 0,
    RT_C_LANDUSE = 1,
    RT_C_PARK = 2,
    RT_C_WATER = 3,
    RT_C_BUILDING = 4,
    RT_C_ROAD_EDGE = 5,
    RT_C_ROAD_MINOR = 6,
    RT_C_ROAD_MAJOR = 7,
    /* 8..15: UI */
    RT_C_SHADOW = 8,
    RT_C_ROUTE = 9,
    RT_C_ROUTE_DONE = 10,
    RT_C_PANEL = 11,
    RT_C_ACCENT = 12,
    RT_C_TEXT = 13,
    RT_C_TEXT_DIM = 14,
    RT_C_HILITE = 15,
    RT_C_COUNT = 16
};

/* Under glass, colours below this index are dimmed toward the panel colour;
 * UI colours from here on stay crisp. */
#define RT_GLASS_TINTED (RT_C_ROUTE_DONE + 1)

#endif /* RT_COLORS_H */
