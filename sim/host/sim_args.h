/*
 * sim_args.h - command line of rotos_sim.
 */
#ifndef SIM_ARGS_H
#define SIM_ARGS_H

#include <stdbool.h>
#include <stdint.h>

#include "sim_config.h"

#define SIM_MAX_SNAPS 32u

typedef struct {
    SimConfig cfg;
    uint32_t snaps[SIM_MAX_SNAPS];
    uint32_t n_snaps;
    const char *snap_dir;
    int32_t theme;             /* -1 = keep stored setting */
    int32_t north_up;          /* -1 = keep stored setting */
    int32_t glass;             /* -1 = keep stored setting */
    uint32_t max_ms;           /* headless safety stop     */
    bool fresh;                /* forget stored map/settings */
} SimArgs;

/* Returns false (after printing usage) on bad arguments. */
bool sim_args_parse(int argc, char **argv, SimArgs *out);

#endif /* SIM_ARGS_H */
