/*
 * rotos_sim - runs the ROTOS firmware application on the desktop.
 *
 * The firmware's own app code (firmware/app) runs unchanged; only the HAL
 * differs: pixels go to the screen twin over virtual SPI, the phone link is
 * a capture replay or a TCP socket, flash is a directory.
 *
 * Start-up order (same as firmware/main/main.c):
 *   args -> sim config -> storage -> display -> input -> link -> app
 * then the frame loop: link -> input -> step -> render -> snapshots -> pace.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "hal_display.h"
#include "hal_input.h"
#include "hal_link.h"
#include "hal_storage.h"
#include "hal_time.h"
#include "rn/rn_types.h"
#include "rt_app.h"
#include "rt_config.h"
#include "sim_args.h"
#include "sim_config.h"
#include "sim_snap.h"

static RtApp g_app;            /* ~115 KB: static, like on the device */
static RnTileCache g_cache;    /* 64 KB tile cache, kept outside RtApp */
static RnPackPoint g_line[RN_ROUTE_MAX_PTS];   /* run-time route lines */

static void forget_flash(const char *dir)
{
    char p[512];
    (void)snprintf(p, sizeof p, "%s/map.rmap", dir);
    (void)unlink(p);
    (void)snprintf(p, sizeof p, "%s/settings.bin", dir);
    (void)unlink(p);
}

static bool init_all(const SimArgs *a)
{
    sim_configure(&a->cfg);
    if (a->fresh) {
        forget_flash(a->cfg.state_dir);
    }
    if (!hal_storage_init()) {
        (void)fprintf(stderr, "storage init failed\n");
        return false;
    }
    if (!hal_display_init()) {
        (void)fprintf(stderr, "no screen on port %u: start ./build/sim/rotos_screen first (or use --headless)\n",
                      (unsigned)a->cfg.screen_port);
        return false;
    }
    if (!hal_input_init() || !hal_link_init()) {
        (void)fprintf(stderr, "input/link init failed (feed file missing?)\n");
        return false;
    }
    rt_app_init(&g_app, &g_cache, g_line, hal_time_ms());
    g_app.reset_reason = 1u;                                   /* the simulator: always a power-on */
    RnSettings s = g_app.settings;
    s.theme_mode = (a->theme >= 0) ? (uint8_t)a->theme : s.theme_mode;
    s.north_up = (a->north_up >= 0) ? (uint8_t)a->north_up : s.north_up;
    s.glass_pct = (a->glass >= 0) ? (uint8_t)a->glass : s.glass_pct;
    rt_app_apply_settings(&g_app, &s, false);
    return true;
}

static void take_snaps(const SimArgs *a, uint32_t now, uint32_t *next)
{
    while (*next < a->n_snaps && now >= a->snaps[*next]) {
        char p[512];
        (void)mkdir(a->snap_dir, 0755);
        (void)snprintf(p, sizeof p, "%s/frame_%06u.ppm", a->snap_dir, (unsigned)a->snaps[*next]);
        bool ok = sim_snap_write(&g_app.presenter, &g_app.canvas, p);
        (void)printf("snap %s %s\n", p, ok ? "" : "(failed)");
        *next += 1u;
    }
}

/* Headless runs end on their own: after the last snapshot, or once the
 * replay is over and the summary card has been shown. */
static bool headless_done(const SimArgs *a, uint32_t now, uint32_t next_snap, uint32_t arrived)
{
    if (now >= a->max_ms) {
        return true;
    }
    if (a->n_snaps > 0u) {
        return next_snap >= a->n_snaps;
    }
    bool replay_over = sim_link_feed_done() && now > sim_link_feed_end_ms() + 5000u;
    return (arrived != 0u && now > arrived + 3000u) || replay_over;
}

static void report(uint32_t frames, uint64_t spi0, uint32_t f0)
{
    const RnParser *p = &g_app.link.parser;
    (void)printf("link: %u packets ok, %u lost, %u crc errors | map transfers %u ok, %u failed\n",
                 (unsigned)p->ok, (unsigned)p->dropped, (unsigned)p->crc_errors,
                 (unsigned)g_app.maprx.completed, (unsigned)g_app.maprx.failed);
    if (frames > f0) {
        double per = (double)(sim_display_bytes() - spi0) / (double)(frames - f0);
        (void)printf("SPI: %.1f KiB/frame avg -> %.1f ms/frame at 40 MHz | tile cache %u hits, %u misses\n",
                     per / 1024.0, per * 8.0 / 40e6 * 1000.0, (unsigned)g_cache.hits,
                     (unsigned)g_cache.misses);
    }
    (void)printf("assertion failures recorded: %u\n", (unsigned)rn_assert_count());
}

int main(int argc, char **argv)
{
    static SimArgs args;
    if (!sim_args_parse(argc, argv, &args) || !init_all(&args)) {
        return 2;
    }
    uint32_t next_snap = 0u, frames = 0u, arrived = 0u, f0 = 0u;
    uint64_t spi0 = 0u;
    RtState last = g_app.state;
    for (;;) {                                         /* JPL-R2 exempt: main loop */
        uint32_t now = hal_time_ms();
        rt_app_service_link(&g_app, now);
        rt_app_service_input(&g_app, now);
        rt_app_step(&g_app, now);
        rt_app_render(&g_app, now);
        take_snaps(&args, now, &next_snap);
        frames++;
        if (g_app.state != last && g_app.state == RT_ST_NAVIGATING && f0 == 0u) {
            f0 = frames;
            spi0 = sim_display_bytes();
        }
        arrived = (g_app.state == RT_ST_SUMMARY && arrived == 0u) ? now : arrived;
        last = g_app.state;
        if (args.cfg.headless ? headless_done(&args, now, next_snap, arrived) : !sim_display_ok()) {
            break;
        }
        hal_time_sleep_until(now + RT_FRAME_MS);
    }
    report(frames, spi0, f0);
    return 0;
}
