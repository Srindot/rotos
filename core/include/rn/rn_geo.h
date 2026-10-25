/*
 * rn_geo.h - geodesy helpers: great-circle distance, bearing, Web Mercator
 * world pixels (256 px tiles), and a local metric frame for fast planar maths.
 */
#ifndef RN_GEO_H
#define RN_GEO_H

#include "rn/rn_types.h"

#define RN_EARTH_R 6378137.0   /* WGS-84 equatorial radius, metres */

/* Local east/north metres around an origin (equirectangular). Accurate to
 * well under 0.5 % across a city; all matching and flyover maths uses it. */
typedef struct {
    double lat0;
    double lon0;
    double m_per_deg_lat;
    double m_per_deg_lon;
} RnLocalFrame;

double rn_geo_haversine_m(double lat1, double lon1, double lat2, double lon2);
/* Initial bearing a -> b, degrees clockwise from north, [0, 360). */
double rn_geo_bearing_deg(double lat1, double lon1, double lat2, double lon2);

/* Web Mercator world pixel at zoom z (256 px tiles). */
void rn_geo_world_px(double lat, double lon, uint8_t zoom, double *x, double *y);
void rn_geo_from_world_px(double x, double y, uint8_t zoom, double *lat, double *lon);
double rn_geo_m_per_px(double lat, uint8_t zoom);

RnLocalFrame rn_geo_frame(double lat0, double lon0);
RnVec2 rn_geo_to_local(const RnLocalFrame *f, double lat, double lon);
void rn_geo_from_local(const RnLocalFrame *f, RnVec2 p, double *lat, double *lon);

/* Smallest angle between two headings in degrees, [0, 180]. */
float rn_geo_angle_diff_deg(float a, float b);
/* Wrap to (-pi, pi]. */
float rn_geo_wrap_pi(float a);

#endif /* RN_GEO_H */
