/*
 * rn_maneuver.h - turn-by-turn maneuver vocabulary. Enum values are part of
 * the map pack format and must never be renumbered.
 */
#ifndef RN_MANEUVER_H
#define RN_MANEUVER_H

#include "rn/rn_types.h"

typedef enum {
    RN_M_NONE = 0,
    RN_M_DEPART = 1,
    RN_M_STRAIGHT = 2,
    RN_M_SLIGHT_LEFT = 3,
    RN_M_LEFT = 4,
    RN_M_SHARP_LEFT = 5,
    RN_M_UTURN_LEFT = 6,
    RN_M_SLIGHT_RIGHT = 7,
    RN_M_RIGHT = 8,
    RN_M_SHARP_RIGHT = 9,
    RN_M_UTURN_RIGHT = 10,
    RN_M_ROUNDABOUT_LEFT = 11,
    RN_M_ROUNDABOUT_RIGHT = 12,
    RN_M_FORK_LEFT = 13,
    RN_M_FORK_RIGHT = 14,
    RN_M_MERGE = 15,
    RN_M_RAMP_LEFT = 16,
    RN_M_RAMP_RIGHT = 17,
    RN_M_NAME_CHANGE = 18,
    RN_M_ARRIVE = 19,
    RN_M_COUNT = 20
} RnManeuverType;

/* Where a flyover splits from the road below (from OSM bridge geometry). */
typedef enum {
    RN_FLY_NONE = 0,
    RN_FLY_TAKE = 1,        /* the route climbs onto a flyover here        */
    RN_FLY_STAY_BELOW = 2,  /* a flyover starts here; the route stays below */
    RN_FLY_EXIT = 3         /* the route comes down off a flyover here     */
} RnFlyoverHint;

typedef struct {
    uint8_t type;                          /* RnManeuverType              */
    uint8_t hint;                          /* RnFlyoverHint               */
    uint16_t point_idx;                    /* route vertex where it happens */
    float along_m;                         /* distance from route start   */
    char road[RN_ROAD_NAME_MAX + 1u];      /* road after the maneuver     */
} RnManeuver;

/* Maneuvers worth announcing (DEPART and NAME_CHANGE are not). */
bool rn_maneuver_is_announced(uint8_t type);
/* Google Routes API maneuver string -> type ("TURN_LEFT" -> RN_M_LEFT).
 * Unknown strings map to RN_M_STRAIGHT. */
uint8_t rn_maneuver_from_google(const char *s);
/* Comfortable speed through the maneuver, m/s (used by the ride simulator);
 * negative when the maneuver does not slow the rider down. */
float rn_maneuver_turn_speed(uint8_t type);
/* Short human name, e.g. "LEFT", for logs and tests. */
const char *rn_maneuver_name(uint8_t type);

/* Upcoming maneuvers as seen from distance `along` on a route. */
typedef struct {
    int32_t next;      /* index of the next announced maneuver, -1 = none   */
    int32_t then;      /* follow-up if it comes within 250 m, -1 = none      */
    int32_t current;   /* last maneuver passed (gives the current road), -1  */
} RnLookahead;

#define RN_THEN_WINDOW_M 250.0f

/* along_of[i] must be sorted ascending; types[i] parallel to it. */
RnLookahead rn_maneuver_lookahead(const float *along_of, const uint8_t *types,
                                  uint32_t n, float along);

#endif /* RN_MANEUVER_H */
