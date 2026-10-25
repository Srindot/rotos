/*
 * rn_sun.h - low-precision solar position (error well under 1 degree),
 * enough to decide day vs night for the automatic theme switch.
 */
#ifndef RN_SUN_H
#define RN_SUN_H

#include "rn/rn_types.h"

/* The sun's disc touches the horizon at -0.833 deg (refraction + radius). */
#define RN_SUN_HORIZON_DEG (-0.833)

/* Elevation of the sun above the horizon, degrees. unix_s = seconds since
 * 1970-01-01 UTC. */
double rn_sun_elevation_deg(double lat, double lon, double unix_s);
bool rn_sun_is_night(double lat, double lon, double unix_s);

/* First day->night crossing in [from_unix_s, from_unix_s + 24 h), searched
 * minute by minute; returns -1 when the sun never sets (polar day). */
double rn_sun_next_sunset(double lat, double lon, double from_unix_s);

#endif /* RN_SUN_H */
