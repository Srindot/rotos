/*
 * rn_matcher.h - map matching: snaps noisy phone GPS fixes onto the route.
 *
 * Improvements over a plain nearest-segment search:
 *   - windowed search around the previous match (cheap, and it cannot jump
 *     to a parallel road or a later part of a looping route)
 *   - heading consistency: when moving, segments pointing the other way are
 *     penalised (stops snapping to the opposite carriageway or the flyover
 *     overhead going the other way)
 *   - off-route hysteresis: off after 3 bad fixes, back on after 2 good ones
 *   - re-acquisition: while off route, a global search can re-lock onto any
 *     part of the route the rider rejoins
 */
#ifndef RN_MATCHER_H
#define RN_MATCHER_H

#include "rn/rn_route.h"

#define RN_MATCH_LOCK_M        50.0f   /* initial lock radius              */
#define RN_MATCH_OFF_M         40.0f   /* cross-track beyond this is "off" */
#define RN_MATCH_OFF_FIXES     3u
#define RN_MATCH_ON_FIXES      2u
#define RN_MATCH_BACK_M        30.0f   /* search window behind the rider   */
#define RN_MATCH_AHEAD_MIN_M   80.0f   /* minimum search window ahead      */

typedef struct {
    float along_m;        /* matched distance along the route              */
    float cross_m;        /* distance from the route at the last fix       */
    bool locked;          /* has a valid match                             */
    bool off_route;
    uint8_t bad_fixes;
    uint8_t good_fixes;
    uint32_t last_t_ms;
} RnMatcher;

typedef struct {
    RnVec2 pos;           /* fix in route->frame metres                    */
    float speed_mps;
    float heading_deg;    /* course over ground                            */
    bool heading_valid;   /* only meaningful when moving                   */
    float accuracy_m;
    uint32_t t_ms;
} RnMatchInput;

void rn_matcher_init(RnMatcher *m);
/* Update with one fix; returns RN_OK, or RN_ERR_STATE for an empty route. */
rn_status rn_matcher_update(RnMatcher *m, const RnRoute *r, const RnMatchInput *in);

/* Projection of p onto segment i of the route: distance and along. */
typedef struct {
    float dist_m;
    float along_m;
    float cost;
} RnProjection;

RnProjection rn_matcher_project(const RnRoute *r, uint32_t seg, RnVec2 p,
                                float heading_deg, bool use_heading);

#endif /* RN_MATCHER_H */
