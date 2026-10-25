/*
 * sim_args.c - command line parsing.
 */
#include "sim_args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char USAGE[] =
    "usage: rotos_sim [--feed FILE.rfeed | --listen PORT] [options]\n"
    "  --feed FILE        replay a virtual-phone capture (sim/phone/rotos_phone.py)\n"
    "  --listen PORT      wait for a live phone on TCP (Android app: 10.0.2.2:PORT)\n"
    "  --headless         no screen twin, virtual clock (fast, deterministic)\n"
    "  --port N           screen twin port (default 9341)\n"
    "  --speed X          run the clock X times faster (windowed mode)\n"
    "  --snap MS,MS,...   save PPM frames at these device times\n"
    "  --snap-dir DIR     where snapshots go (default out/snaps)\n"
    "  --theme auto|day|night   --north-up   --glass PCT\n"
    "  --press A@MS[:HOLD],...  scripted button presses (A B C)\n"
    "  --state-dir DIR    simulated flash (default out/state)\n"
    "  --fresh            start with empty flash (no stored map / settings)\n"
    "  --max-s S          headless: stop after S seconds of device time\n";

static uint32_t parse_list(const char *s, uint32_t *out, uint32_t cap)
{
    uint32_t n = 0u;
    const char *p = s;
    for (uint32_t k = 0u; k < cap && p != NULL && *p != '\0'; k++) {
        out[n] = (uint32_t)strtoul(p, NULL, 10);
        n++;
        p = strchr(p, ',');
        p = (p != NULL) ? p + 1 : NULL;
    }
    return n;
}

static uint32_t parse_presses(const char *s, SimPress *out, uint32_t cap)
{
    uint32_t n = 0u;
    const char *p = s;
    for (uint32_t k = 0u; k < cap && p != NULL && *p != '\0'; k++) {
        const char *at = strchr(p, '@');
        if (at == NULL) {
            break;
        }
        out[n].button = p[0];
        out[n].at_ms = (uint32_t)strtoul(at + 1, NULL, 10);
        const char *colon = strchr(at, ':');
        const char *comma = strchr(at, ',');
        bool hold = colon != NULL && (comma == NULL || colon < comma);
        out[n].hold_ms = hold ? (uint32_t)strtoul(colon + 1, NULL, 10) : 150u;
        n++;
        p = (comma != NULL) ? comma + 1 : NULL;
    }
    return n;
}

static int32_t theme_of(const char *s)
{
    return (strcmp(s, "day") == 0) ? 1 : ((strcmp(s, "night") == 0) ? 2 : 0);
}

static bool option(const char *a, const char *v, SimArgs *o)
{
    if (strcmp(a, "--feed") == 0) { o->cfg.link_mode = SIM_LINK_FEED; o->cfg.feed_path = v; }
    else if (strcmp(a, "--listen") == 0) { o->cfg.link_mode = SIM_LINK_TCP; o->cfg.listen_port = (uint16_t)atoi(v); }
    else if (strcmp(a, "--port") == 0) { o->cfg.screen_port = (uint16_t)atoi(v); }
    else if (strcmp(a, "--speed") == 0) { o->cfg.speed = (float)atof(v); }
    else if (strcmp(a, "--snap") == 0) { o->n_snaps = parse_list(v, o->snaps, SIM_MAX_SNAPS); }
    else if (strcmp(a, "--snap-dir") == 0) { o->snap_dir = v; }
    else if (strcmp(a, "--theme") == 0) { o->theme = theme_of(v); }
    else if (strcmp(a, "--glass") == 0) { o->glass = atoi(v); }
    else if (strcmp(a, "--press") == 0) { o->cfg.n_presses = parse_presses(v, o->cfg.presses, SIM_MAX_PRESSES); }
    else if (strcmp(a, "--state-dir") == 0) { o->cfg.state_dir = v; }
    else if (strcmp(a, "--max-s") == 0) { o->max_ms = (uint32_t)atoi(v) * 1000u; }
    else { return false; }
    return true;
}

bool sim_args_parse(int argc, char **argv, SimArgs *o)
{
    (void)memset(o, 0, sizeof *o);
    o->cfg = *sim_config();
    o->snap_dir = "out/snaps";
    o->theme = -1;
    o->north_up = -1;
    o->glass = -1;
    o->max_ms = 3600u * 1000u;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--headless") == 0) { o->cfg.headless = true; continue; }
        if (strcmp(a, "--north-up") == 0) { o->north_up = 1; continue; }
        if (strcmp(a, "--fresh") == 0) { o->fresh = true; continue; }
        if (i + 1 < argc && option(a, argv[i + 1], o)) { i++; continue; }
        (void)fprintf(stderr, "unknown option %s\n%s", a, USAGE);
        return false;
    }
    o->cfg.speed = (o->cfg.speed > 0.0f) ? o->cfg.speed : 1.0f;
    return true;
}
