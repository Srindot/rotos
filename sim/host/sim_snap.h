/*
 * sim_snap.h - frame snapshots as PPM, from exactly the pixels the panel
 * receives (rt_present_row).
 */
#ifndef SIM_SNAP_H
#define SIM_SNAP_H

#include <stdbool.h>

#include "rt_canvas.h"
#include "rt_present.h"

bool sim_snap_write(const RtPresenter *p, const RtCanvas *cv, const char *path);

#endif /* SIM_SNAP_H */
