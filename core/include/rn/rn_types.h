/*
 * rn_types.h - basic types, limits and the assertion macro shared by every
 * ROTOS module (navigation library, firmware, simulator, Android JNI).
 *
 * All sizes are compile-time limits (JPL rule 3: no dynamic memory). Raising a
 * limit costs RAM only where the corresponding struct is instantiated.
 */
#ifndef RN_TYPES_H
#define RN_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- status codes ------------------------------------------------------ */
typedef int32_t rn_status;
#define RN_OK          0
#define RN_ERR_PARAM  (-1)   /* invalid argument                           */
#define RN_ERR_RANGE  (-2)   /* value outside a supported limit            */
#define RN_ERR_FORMAT (-3)   /* malformed input data                       */
#define RN_ERR_CRC    (-4)   /* checksum mismatch                          */
#define RN_ERR_FULL   (-5)   /* output buffer / table capacity exhausted   */
#define RN_ERR_STATE  (-6)   /* call not valid in the current state        */

/* ---- limits -------------------------------------------------------------- */
#define RN_MAX_ROUTE_PTS    8192u   /* decoded route geometry points        */
#define RN_MAX_MANEUVERS     256u   /* incl. synthetic flyover maneuvers    */
#define RN_ROAD_NAME_MAX      40u   /* chars, excluding the terminating NUL */
#define RN_MAX_BRIDGES       256u   /* OSM bridge ways near a route         */
#define RN_MAX_BRIDGE_PTS   4096u   /* all bridge vertices together         */
#define RN_MAX_ROUTE_M   200000.0f  /* longest supported route, metres     */
#define RN_MAX_POLYLINE_LEN 65536u  /* encoded polyline string length       */

/* ---- maths -------------------------------------------------------------- */
#define RN_PI       3.14159265358979323846
#define RN_PI_F     3.14159265f
#define RN_DEG2RAD  (RN_PI / 180.0)
#define RN_RAD2DEG  (180.0 / RN_PI)

/* ---- assertions (JPL rule 5) ---------------------------------------------
 * RN_ASSERT(e) evaluates to true when e holds. When it does not, the failure
 * is recorded (file, line, count) and the expression evaluates to false so
 * the caller can recover - typically `if (!RN_ASSERT(p != NULL)) { return
 * RN_ERR_PARAM; }`. Nothing aborts on the device. Host test builds define
 * RN_ASSERT_ABORT to stop at the first failure instead.
 */
bool rn_assert_fail(const char *file, int32_t line);
uint32_t rn_assert_count(void);
const char *rn_assert_last_file(void);
int32_t rn_assert_last_line(void);
void rn_assert_reset(void);

#define RN_ASSERT(e) ((e) ? true : rn_assert_fail(__FILE__, (int32_t)__LINE__))

/* ---- small value types --------------------------------------------------- */
typedef struct {
    float x;
    float y;
} RnVec2;

#endif /* RN_TYPES_H */
