/*
 * rn_sun.c - solar elevation (Astronomical Almanac low-precision formulae).
 */
#include "rn/rn_sun.h"

#include <math.h>

#define J2000_UNIX_S 946728000.0   /* 2000-01-01 12:00 UTC */
#define MINUTES_PER_DAY 1440u

double rn_sun_elevation_deg(double lat, double lon, double unix_s)
{
    (void)RN_ASSERT(lat >= -90.0 && lat <= 90.0);
    (void)RN_ASSERT(isfinite(unix_s));
    double d = (unix_s - J2000_UNIX_S) / 86400.0;
    double g = (357.529 + 0.98560028 * d) * RN_DEG2RAD;
    double q = 280.459 + 0.98564736 * d;
    double l = (q + 1.915 * sin(g) + 0.020 * sin(2.0 * g)) * RN_DEG2RAD;
    double e = (23.439 - 0.00000036 * d) * RN_DEG2RAD;
    double ra = atan2(cos(e) * sin(l), cos(l));
    double dec = asin(sin(e) * sin(l));
    double gmst_h = fmod(18.697374558 + 24.06570982441908 * d, 24.0);
    double ha = (gmst_h * 15.0 + lon) * RN_DEG2RAD - ra;
    double la = lat * RN_DEG2RAD;
    double s = sin(la) * sin(dec) + cos(la) * cos(dec) * cos(ha);
    s = (s > 1.0) ? 1.0 : ((s < -1.0) ? -1.0 : s);
    return asin(s) * RN_RAD2DEG;
}

bool rn_sun_is_night(double lat, double lon, double unix_s)
{
    return rn_sun_elevation_deg(lat, lon, unix_s) < RN_SUN_HORIZON_DEG;
}

double rn_sun_next_sunset(double lat, double lon, double from_unix_s)
{
    if (!RN_ASSERT(isfinite(from_unix_s))) {
        return -1.0;
    }
    bool was_night = rn_sun_is_night(lat, lon, from_unix_s);
    for (uint32_t m = 1u; m <= MINUTES_PER_DAY; m++) {
        double t = from_unix_s + (double)m * 60.0;
        bool night = rn_sun_is_night(lat, lon, t);
        if (night && !was_night) {
            return t;
        }
        was_night = night;
    }
    return -1.0;
}
