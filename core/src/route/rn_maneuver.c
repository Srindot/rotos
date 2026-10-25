/*
 * rn_maneuver.c - maneuver vocabulary and lookahead.
 */
#include "rn/rn_maneuver.h"

#include <string.h>

typedef struct {
    const char *google;
    uint8_t type;
} GoogleName;

static const GoogleName GOOGLE_NAMES[] = {
    {"DEPART", RN_M_DEPART},
    {"STRAIGHT", RN_M_STRAIGHT},
    {"TURN_SLIGHT_LEFT", RN_M_SLIGHT_LEFT},
    {"TURN_LEFT", RN_M_LEFT},
    {"TURN_SHARP_LEFT", RN_M_SHARP_LEFT},
    {"UTURN_LEFT", RN_M_UTURN_LEFT},
    {"TURN_SLIGHT_RIGHT", RN_M_SLIGHT_RIGHT},
    {"TURN_RIGHT", RN_M_RIGHT},
    {"TURN_SHARP_RIGHT", RN_M_SHARP_RIGHT},
    {"UTURN_RIGHT", RN_M_UTURN_RIGHT},
    {"ROUNDABOUT_LEFT", RN_M_ROUNDABOUT_LEFT},
    {"ROUNDABOUT_RIGHT", RN_M_ROUNDABOUT_RIGHT},
    {"FORK_LEFT", RN_M_FORK_LEFT},
    {"FORK_RIGHT", RN_M_FORK_RIGHT},
    {"MERGE", RN_M_MERGE},
    {"RAMP_LEFT", RN_M_RAMP_LEFT},
    {"RAMP_RIGHT", RN_M_RAMP_RIGHT},
    {"NAME_CHANGE", RN_M_NAME_CHANGE},
    {"ARRIVE", RN_M_ARRIVE},
};

static const char *const NAMES[RN_M_COUNT] = {
    "NONE", "DEPART", "STRAIGHT", "SLIGHT LEFT", "LEFT", "SHARP LEFT", "U-TURN LEFT",
    "SLIGHT RIGHT", "RIGHT", "SHARP RIGHT", "U-TURN RIGHT", "ROUNDABOUT L",
    "ROUNDABOUT R", "FORK LEFT", "FORK RIGHT", "MERGE", "RAMP LEFT", "RAMP RIGHT",
    "NAME CHANGE", "ARRIVE",
};

/* Turn speeds in m/s, indexed by type; < 0 = no slow-down. */
static const float TURN_SPEED[RN_M_COUNT] = {
    -1.0f, -1.0f, -1.0f, 7.0f, 4.5f, 3.5f, 2.5f, 7.0f, 4.5f, 3.5f, 2.5f,
    4.5f, 4.5f, 8.0f, 8.0f, 8.0f, 7.0f, 7.0f, -1.0f, 0.0f,
};

bool rn_maneuver_is_announced(uint8_t type)
{
    return type != (uint8_t)RN_M_NONE && type != (uint8_t)RN_M_DEPART &&
           type != (uint8_t)RN_M_NAME_CHANGE && type < (uint8_t)RN_M_COUNT;
}

uint8_t rn_maneuver_from_google(const char *s)
{
    if (!RN_ASSERT(s != NULL)) {
        return (uint8_t)RN_M_STRAIGHT;
    }
    uint32_t n = (uint32_t)(sizeof GOOGLE_NAMES / sizeof GOOGLE_NAMES[0]);
    for (uint32_t i = 0u; i < n; i++) {
        if (strncmp(s, GOOGLE_NAMES[i].google, 24u) == 0) {
            return GOOGLE_NAMES[i].type;
        }
    }
    return (uint8_t)RN_M_STRAIGHT;
}

float rn_maneuver_turn_speed(uint8_t type)
{
    if (!RN_ASSERT(type < (uint8_t)RN_M_COUNT)) {
        return -1.0f;
    }
    return TURN_SPEED[type];
}

const char *rn_maneuver_name(uint8_t type)
{
    if (!RN_ASSERT(type < (uint8_t)RN_M_COUNT)) {
        return "?";
    }
    return NAMES[type];
}

RnLookahead rn_maneuver_lookahead(const float *along_of, const uint8_t *types,
                                  uint32_t n, float along)
{
    RnLookahead la = {-1, -1, -1};
    if (!RN_ASSERT(along_of != NULL && types != NULL) || !RN_ASSERT(n <= RN_MAX_MANEUVERS)) {
        return la;
    }
    for (uint32_t i = 0u; i < n; i++) {
        if (along_of[i] <= along + 1.0f) {
            la.current = (int32_t)i;          /* already passed it */
            continue;
        }
        if (!rn_maneuver_is_announced(types[i])) {
            continue;
        }
        if (la.next < 0) {
            la.next = (int32_t)i;
            continue;
        }
        if (along_of[i] - along_of[la.next] < RN_THEN_WINDOW_M) {
            la.then = (int32_t)i;
        }
        break;
    }
    return la;
}
