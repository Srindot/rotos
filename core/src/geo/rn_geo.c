/*
 * rn_geo.c - geodesy helpers.
 */
#include "rn/rn_geo.h"

#include <math.h>

double rn_geo_haversine_m(double lat1, double lon1, double lat2, double lon2)
{
    double p1 = lat1 * RN_DEG2RAD;
    double p2 = lat2 * RN_DEG2RAD;
    double dp = (lat2 - lat1) * RN_DEG2RAD;
    double dl = (lon2 - lon1) * RN_DEG2RAD;
    double h = sin(dp * 0.5) * sin(dp * 0.5) + cos(p1) * cos(p2) * sin(dl * 0.5) * sin(dl * 0.5);
    h = (h > 1.0) ? 1.0 : ((h < 0.0) ? 0.0 : h);
    return 2.0 * RN_EARTH_R * asin(sqrt(h));
}

double rn_geo_bearing_deg(double lat1, double lon1, double lat2, double lon2)
{
    double p1 = lat1 * RN_DEG2RAD;
    double p2 = lat2 * RN_DEG2RAD;
    double dl = (lon2 - lon1) * RN_DEG2RAD;
    double y = sin(dl) * cos(p2);
    double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
    double b = atan2(y, x) * RN_RAD2DEG;
    return fmod(b + 360.0, 360.0);
}

void rn_geo_world_px(double lat, double lon, uint8_t zoom, double *x, double *y)
{
    if (!RN_ASSERT(x != NULL && y != NULL) || !RN_ASSERT(zoom <= 22u)) {
        return;
    }
    double n = 256.0 * (double)(1u << zoom);
    double la = (lat > 85.05) ? 85.05 : ((lat < -85.05) ? -85.05 : lat);
    double s = sin(la * RN_DEG2RAD);
    *x = (lon + 180.0) / 360.0 * n;
    *y = (0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * RN_PI)) * n;
}

void rn_geo_from_world_px(double x, double y, uint8_t zoom, double *lat, double *lon)
{
    if (!RN_ASSERT(lat != NULL && lon != NULL) || !RN_ASSERT(zoom <= 22u)) {
        return;
    }
    double n = 256.0 * (double)(1u << zoom);
    *lon = x / n * 360.0 - 180.0;
    *lat = atan(sinh(RN_PI * (1.0 - 2.0 * y / n))) * RN_RAD2DEG;
}

double rn_geo_m_per_px(double lat, uint8_t zoom)
{
    if (!RN_ASSERT(zoom <= 22u)) {
        return 1.0;
    }
    return 2.0 * RN_PI * RN_EARTH_R * cos(lat * RN_DEG2RAD) / (256.0 * (double)(1u << zoom));
}

RnLocalFrame rn_geo_frame(double lat0, double lon0)
{
    RnLocalFrame f;
    (void)RN_ASSERT(lat0 >= -90.0 && lat0 <= 90.0);
    f.lat0 = lat0;
    f.lon0 = lon0;
    f.m_per_deg_lat = 110540.0;
    f.m_per_deg_lon = 111320.0 * cos(lat0 * RN_DEG2RAD);
    return f;
}

RnVec2 rn_geo_to_local(const RnLocalFrame *f, double lat, double lon)
{
    RnVec2 p = {0.0f, 0.0f};
    if (!RN_ASSERT(f != NULL)) {
        return p;
    }
    p.x = (float)((lon - f->lon0) * f->m_per_deg_lon);
    p.y = (float)((lat - f->lat0) * f->m_per_deg_lat);
    return p;
}

void rn_geo_from_local(const RnLocalFrame *f, RnVec2 p, double *lat, double *lon)
{
    if (!RN_ASSERT(f != NULL && lat != NULL && lon != NULL) ||
        !RN_ASSERT(f->m_per_deg_lon > 0.0)) {
        return;
    }
    *lat = f->lat0 + (double)p.y / f->m_per_deg_lat;
    *lon = f->lon0 + (double)p.x / f->m_per_deg_lon;
}

float rn_geo_angle_diff_deg(float a, float b)
{
    float d = fmodf(fabsf(a - b), 360.0f);
    return (d > 180.0f) ? 360.0f - d : d;
}

float rn_geo_wrap_pi(float a)
{
    float r = remainderf(a, 2.0f * RN_PI_F);
    return (r <= -RN_PI_F) ? r + 2.0f * RN_PI_F : r;
}
