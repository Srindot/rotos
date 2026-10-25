/*
 * sim_config.c - holds the simulator configuration.
 */
#include "sim_config.h"

#include <string.h>

static SimConfig s_cfg = {false, 9341u, SIM_LINK_NONE, NULL, 2600u, 7400u, "out/state",
                          3u * 1024u * 1024u, 1.0f, {{'A', 0u, 0u}}, 0u};

void sim_configure(const SimConfig *c)
{
    if (c != NULL) {
        s_cfg = *c;
    }
}

const SimConfig *sim_config(void)
{
    return &s_cfg;
}
