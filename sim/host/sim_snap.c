/*
 * sim_snap.c - PPM snapshots.
 */
#include "sim_snap.h"

#include <stdio.h>

bool sim_snap_write(const RtPresenter *p, const RtCanvas *cv, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    (void)fprintf(f, "P6\n%d %d\n255\n", RT_SCREEN_W, RT_SCREEN_H);
    uint16_t row[RT_SCREEN_W];
    for (int32_t y = 0; y < RT_SCREEN_H; y++) {
        rt_present_row(p, cv, y, row);
        for (int32_t x = 0; x < RT_SCREEN_W; x++) {
            uint8_t px[3];
            rt_rgb565_split(row[x], &px[0], &px[1], &px[2]);
            (void)fwrite(px, 1u, 3u, f);
        }
    }
    return fclose(f) == 0;
}
