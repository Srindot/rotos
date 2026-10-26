/*
 * rn_matcher.c - map matching.
 */
#include "rn/rn_matcher.h"

#include <math.h>
#include <string.h>

#define WRONG_WAY_DEG     100.0f   /* segment vs course beyond this...      */
#define WRONG_WAY_COST     35.0f   /* ...costs as much as 35 m of distance  */
#define HEADING_MIN_MPS     3.0f   /* GPS course is noise below this speed  */
#define MAX_GAP_S          10.0f

void rn_matcher_init(RnMatcher *m)
{
    if (!RN_ASSERT(m != NULL)) {
        return;
    }
    (void)memset(m, 0, sizeof *m);
}

RnProjection rn_matcher_project(const RnRoute *r, uint32_t seg, RnVec2 p,
                                float heading_deg, bool use_heading)
{
    RnProjection out = {1e9f, 0.0f, 1e9f};
    if (!RN_ASSERT(r != NULL) || !RN_ASSERT(seg + 1u < r->n_pts)) {
        return out;
    }
    RnVec2 a = r->pts[seg].xy;
    RnVec2 b = r->pts[seg + 1u].xy;
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float l2 = dx * dx + dy * dy;
    float t = (l2 > 1e-6f) ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l2 : 0.0f;
    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    float ex = a.x + dx * t - p.x;
    float ey = a.y + dy * t - p.y;
    float seg_len = r->pts[seg + 1u].along_m - r->pts[seg].along_m;
    out.dist_m = sqrtf(ex * ex + ey * ey);
    out.along_m = r->pts[seg].along_m + seg_len * t;
    out.cost = out.dist_m;
    if (use_heading && l2 > 1.0f) {
        float seg_deg = atan2f(dx, dy) * (float)RN_RAD2DEG;
        if (rn_geo_angle_diff_deg(seg_deg, heading_deg) > WRONG_WAY_DEG) {
            out.cost += WRONG_WAY_COST;
        }
    }
    return out;
}

/* Acquiring: candidates whose cost is within this of the best one are
 * equally good (start and end of a loop, the same street passed twice); of
 * those, the one nearest along the route to where the rider should be wins.
 * Anything clearly nearer the rider always wins. */
#define ACQUIRE_TIE_M 15.0f
/* ... but only a separate pass of the route counts as a tie, never the
 * neighbouring segments of the same street (they would pull the match off
 * the rider's position along it). */
#define ACQUIRE_PASS_M 60.0f

/* Best projection over segments overlapping [lo, hi] metres; with pref >= 0,
 * near-ties are broken by distance along the route from pref. */
static RnProjection search(const RnRoute *r, float lo, float hi, const RnMatchInput *in, float pref)
{
    bool use_heading = in->heading_valid && in->speed_mps >= HEADING_MIN_MPS;
    RnProjection best = {1e9f, 0.0f, 1e9f};
    uint32_t first = rn_route_segment_at(r, lo);
    for (uint32_t i = first; i + 1u < r->n_pts && i < RN_MAX_ROUTE_PTS; i++) {
        if (r->pts[i].along_m > hi) {
            break;
        }
        RnProjection pr = rn_matcher_project(r, i, in->pos, in->heading_deg, use_heading);
        if (pr.cost < best.cost) {
            best = pr;
        }
    }
    if (pref < 0.0f || best.cost >= 1e9f) {
        return best;
    }
    RnProjection pick = best;                         /* second pass: the near-ties */
    for (uint32_t i = first; i + 1u < r->n_pts && i < RN_MAX_ROUTE_PTS; i++) {
        if (r->pts[i].along_m > hi) {
            break;
        }
        RnProjection pr = rn_matcher_project(r, i, in->pos, in->heading_deg, use_heading);
        bool other_pass = fabsf(pr.along_m - best.along_m) > ACQUIRE_PASS_M;
        if (other_pass && pr.cost <= best.cost + ACQUIRE_TIE_M &&
            fabsf(pr.along_m - pref) < fabsf(pick.along_m - pref)) {
            pick = pr;
        }
    }
    return pick;
}

static void update_hysteresis(RnMatcher *m, float off_limit)
{
    if (m->cross_m > off_limit) {
        m->good_fixes = 0u;
        if (m->bad_fixes < 255u) {
            m->bad_fixes++;
        }
        if (m->bad_fixes >= RN_MATCH_OFF_FIXES) {
            m->off_route = true;
        }
    } else {
        m->bad_fixes = 0u;
        if (m->good_fixes < 255u) {
            m->good_fixes++;
        }
        if (m->good_fixes >= RN_MATCH_ON_FIXES) {
            m->off_route = false;
        }
    }
}

rn_status rn_matcher_update(RnMatcher *m, const RnRoute *r, const RnMatchInput *in)
{
    if (!RN_ASSERT(m != NULL && r != NULL && in != NULL)) {
        return RN_ERR_PARAM;
    }
    if (r->n_pts < 2u) {
        return RN_ERR_STATE;
    }
    float off_limit = fmaxf(RN_MATCH_OFF_M, 2.0f * in->accuracy_m);
    RnProjection best;
    if (!m->locked || m->off_route) {
        best = search(r, 0.0f, r->total_m, in, m->locked ? m->along_m : 0.0f);   /* (re)acquire */
        bool accept = best.dist_m < (m->locked ? off_limit * 0.6f : RN_MATCH_LOCK_M);
        if (!accept) {
            /* a rejected re-lock is a bad fix, never a good one: else OFF ROUTE clears with a
             * stale position, flickers, and the reroute timer never runs out */
            m->cross_m = fmaxf(best.dist_m, off_limit + 1.0f);
            update_hysteresis(m, off_limit);
            m->last_t_ms = in->t_ms;
            return RN_OK;                                 /* keep last along */
        }
    } else {
        float dt = (float)(in->t_ms - m->last_t_ms) * 0.001f;
        dt = (dt < 0.0f) ? 0.0f : ((dt > MAX_GAP_S) ? MAX_GAP_S : dt);
        float ahead = fmaxf(RN_MATCH_AHEAD_MIN_M, in->speed_mps * dt * 2.5f + 40.0f);
        best = search(r, m->along_m - RN_MATCH_BACK_M, m->along_m + ahead, in, -1.0f);
        if (best.dist_m > off_limit) {                /* a stray fix: count it, keep the position */
            m->cross_m = best.dist_m;
            m->last_t_ms = in->t_ms;
            update_hysteresis(m, off_limit);
            return RN_OK;
        }
    }
    m->locked = true;
    m->cross_m = best.dist_m;
    m->along_m = best.along_m;
    m->last_t_ms = in->t_ms;
    update_hysteresis(m, off_limit);
    return RN_OK;
}
