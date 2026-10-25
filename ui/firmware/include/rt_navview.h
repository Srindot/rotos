/*
 * rt_navview.h - the navigation view: camera, map, route, rider and HUD,
 * computed from the map pack and the tracker.
 */
#ifndef RT_NAVVIEW_H
#define RT_NAVVIEW_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_mappack.h"
#include "rn/rn_protocol.h"
#include "rn/rn_tracker.h"
#include "rn/rn_line.h"
#include "rt_canvas.h"
#include "rt_theme.h"
#include "rt_widgets.h"

typedef struct {
    bool north_up;
    bool overlays;         /* rider marker + HUD (off under the summary card) */
    bool link_lost;        /* no packets from the phone                        */
    bool gps_stale;        /* packets arrive, positions do not                 */
    RtStatusView status;
    const RnGuidance *guide; /* Google Maps' next turn (free mode HUD), or NULL */
    uint32_t guide_age_ms;   /* since that GUIDANCE arrived (turn countdown)     */
    const RnLine *line;      /* the line to draw: a run-time route, or NULL = the pack's */
    const char *busy;        /* "REROUTING" / "UPDATING MAP" chip, or NULL         */
    bool have_clock;         /* time_s valid                                     */
    uint32_t time_s;         /* local seconds of day, now                        */
} RtNavViewOpts;

void rt_navview_draw(RtCanvas *cv, const RtTheme *t, const RnPack *pack, RnTileCache *cache,
                     const RnTracker *trk, const RtNavViewOpts *opt, uint32_t now_ms);

#endif /* RT_NAVVIEW_H */
