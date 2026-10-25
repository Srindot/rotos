# ROTOS core: the one list of navigation-library sources, included by every
# build (host CMake, ESP-IDF firmware, Android JNI) so nothing is copied.
#   RN_CORE_SOURCES   every platform (also the ESP32 firmware)
#   RN_PHONE_SOURCES  phone-side algorithms (Android app, virtual phone)
#   RN_FFI_SOURCES    flat FFI for JNI / Python ctypes
set(RN_DIR ${CMAKE_CURRENT_LIST_DIR})
set(RN_INCLUDE_DIR ${RN_DIR}/include)
set(RN_FFI_INCLUDE_DIR ${RN_DIR}/src/ffi)

set(RN_CORE_SOURCES
    ${RN_DIR}/src/base/rn_assert.c
    ${RN_DIR}/src/base/rn_wire.c
    ${RN_DIR}/src/base/rn_crc.c
    ${RN_DIR}/src/base/rn_rle.c
    ${RN_DIR}/src/base/rn_text.c
    ${RN_DIR}/src/base/rn_inflate.c
    ${RN_DIR}/src/link/rn_protocol.c
    ${RN_DIR}/src/geo/rn_geo.c
    ${RN_DIR}/src/route/rn_maneuver.c
    ${RN_DIR}/src/route/rn_tracker.c
    ${RN_DIR}/src/route/rn_line.c
    ${RN_DIR}/src/map/rn_mappack.c
    ${RN_DIR}/src/map/rn_delta.c)

set(RN_PHONE_SOURCES
    ${RN_DIR}/src/geo/rn_polyline.c
    ${RN_DIR}/src/geo/rn_sun.c
    ${RN_DIR}/src/route/rn_route.c
    ${RN_DIR}/src/route/rn_matcher.c
    ${RN_DIR}/src/route/rn_engine.c
    ${RN_DIR}/src/route/rn_ridesim.c
    ${RN_DIR}/src/map/rn_flyover.c
    ${RN_DIR}/src/map/rn_corridor.c
    ${RN_DIR}/src/map/rn_packwriter.c
    ${RN_DIR}/src/map/rn_mapgen.c
    ${RN_DIR}/src/map/rn_mapgen_classify.c
    ${RN_DIR}/src/map/rn_mapgen_vote.c
    ${RN_DIR}/src/map/rn_mapgen_filters.c
    ${RN_DIR}/src/map/rn_mapgen_thin.c
    ${RN_DIR}/src/map/rn_delta_enc.c)

set(RN_FFI_SOURCES
    ${RN_DIR}/src/ffi/rn_ffi_route.c
    ${RN_DIR}/src/ffi/rn_ffi_pack.c
    ${RN_DIR}/src/ffi/rn_ffi_link.c
    ${RN_DIR}/src/ffi/rn_ffi_sim.c)
