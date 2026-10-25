/*
 * rn_mapgen.c - the map generation pipeline for one block.
 */
#include "rn/rn_mapgen.h"

#include <string.h>

rn_status rn_mapgen_block(const uint8_t *rgb, uint32_t w_in, uint32_t h_in,
                          uint8_t *out, RnMapgenWork *work)
{
    if (!RN_ASSERT(rgb != NULL && out != NULL && work != NULL)) {
        return RN_ERR_PARAM;
    }
    if (w_in < 2u || h_in < 2u || w_in > RN_MAPGEN_MAX_IN || h_in > RN_MAPGEN_MAX_IN ||
        (w_in & 1u) != 0u || (h_in & 1u) != 0u) {
        return RN_ERR_RANGE;
    }
    RnPlane a = {work->a, w_in / 2u, h_in / 2u};
    rn_mapgen_classify(rgb, w_in * h_in, work->hi, work);    /* zoom z+1     */
    rn_mapgen_downsample2(work->hi, w_in, h_in, &a);          /* -> zoom z    */
    rn_mapgen_inpaint(&a, work->b, 30u, (uint8_t)RN_MAPGEN_ALL_CLASSES);
    rn_mapgen_harmonize_roads(&a, work->b, 2u);
    if (work->source != (uint8_t)RN_MAPGEN_SRC_GOOGLE) {
        rn_mapgen_thin_roads(&a, work);                       /* OSM footpaths, label halos */
    }
    rn_mapgen_despeckle(&a, work->b, 2u);
    rn_mapgen_outline_roads(&a, work->b);
    (void)memcpy(out, a.px, a.w * a.h);
    return RN_OK;
}
