/*
 * rn_ffi_ctx.h - private: the single static navigation session behind the
 * flat FFI API (see rn/rn_ffi.h). Not part of the public include path.
 */
#ifndef RN_FFI_CTX_H
#define RN_FFI_CTX_H

#include "rn/rn_corridor.h"
#include "rn/rn_engine.h"
#include "rn/rn_ffi.h"
#include "rn/rn_flyover.h"
#include "rn/rn_mapgen.h"
#include "rn/rn_mappack.h"
#include "rn/rn_packwriter.h"
#include "rn/rn_protocol.h"
#include "rn/rn_ridesim.h"
#include "rn/rn_route.h"

typedef struct {
    RnRoute route;
    RnBridgeSet bridges;
    RnFlyoverWork fly;
    RnCorridor corridor;
    bool corridor_ok;
    RnMapgenWork mapgen;
    RnPackWriter writer;
    bool writer_open;
    uint8_t pack[RN_FFI_PACK_CAP];
    uint32_t pack_len;
    uint32_t pack_crc;
    bool pack_ok;
    RnPack pack_view;
    uint8_t wire[RN_FFI_PACK_CAP];    /* the pack compressed for the transfer */
    uint32_t wire_len;                /* 0 = send the pack as is              */
    uint32_t wire_base_crc;           /* != 0: the wire is a patch against the pack with this CRC */
    RnTileCache cache;
    RnEngine engine;
    RnNavState nav;
    bool have_nav;
    RnParser rx;
    RnPacket rxq[RN_FFI_RX_QUEUE];
    uint32_t rx_head;
    uint32_t rx_count;
    RnRideSim sim;
    bool sim_ok;
    double scratch_lat[RN_MAX_ROUTE_PTS];
    double scratch_lon[RN_MAX_ROUTE_PTS];
} RnFfiCtx;

RnFfiCtx *rn_ffi_ctx(void);

#endif /* RN_FFI_CTX_H */
