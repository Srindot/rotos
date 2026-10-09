// Navigation model shared by the protocol, the tracker and the renderers.
// Enum values are on the wire: keep in sync with phone/rotos_phone/google_route.py
#pragma once
#include <cstdint>

namespace rotos {

enum ManeuverType : uint8_t {
    M_NONE, M_DEPART, M_STRAIGHT,
    M_SLIGHT_LEFT, M_LEFT, M_SHARP_LEFT, M_UTURN_LEFT,
    M_SLIGHT_RIGHT, M_RIGHT, M_SHARP_RIGHT, M_UTURN_RIGHT,
    M_ROUNDABOUT_LEFT, M_ROUNDABOUT_RIGHT, M_FORK_LEFT, M_FORK_RIGHT,
    M_MERGE, M_RAMP_LEFT, M_RAMP_RIGHT, M_NAME_CHANGE, M_ARRIVE,
};

// Maneuvers worth announcing to the rider (DEPART / NAME_CHANGE are not).
inline bool isAnnounced(ManeuverType t) { return t != M_NONE && t != M_DEPART && t != M_NAME_CHANGE; }

// Where a flyover splits from the road below (set by the phone from OSM).
enum FlyoverHint : uint8_t { FLY_NONE, FLY_TAKE, FLY_STAY_BELOW, FLY_EXIT };

enum NavFlags : uint8_t { NAV_GPS_OK = 1, NAV_OFF_ROUTE = 2, NAV_REROUTING = 4, NAV_NIGHT = 8 };

// NAVIGATION_STATE payload, as sent by the phone at ~1 Hz.
struct NavState {
    uint32_t t_ms;           // phone clock
    double lat, lon;
    float speed_mps;
    float heading_deg;
    float along_m;           // phone's map-matched distance along the route
    float remaining_m;
    uint32_t remaining_s;
    uint32_t eta_s;          // local time of day, seconds since midnight
    uint8_t flags;
    uint8_t accuracy_m;
};

// PHONE_STATUS payload, every ~5 s.
enum StatusFlags : uint8_t { STATUS_CHARGING = 1, STATUS_NIGHT = 2 };
struct PhoneStatus {
    uint32_t t_ms;
    uint32_t time_s;         // local time of day
    uint8_t weekday;         // 0 = Monday
    uint8_t day, month;
    uint8_t battery_pct;
    uint8_t flags;
    uint8_t signal_bars;     // mobile network, 0..4
};

// WEATHER payload: current conditions. code = WMO weather code (Open-Meteo).
struct Weather {
    uint32_t t_ms;
    float temp_c;
    uint8_t code;
    uint8_t rain_prob;
    uint8_t wind_kmh;
    bool is_day;
};

} // namespace rotos
