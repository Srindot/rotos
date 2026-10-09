// Heads-up display: floating panels over the map, plus the full-screen
// boot / waiting states.
#pragma once
#include "canvas.h"
#include "nav.h"

namespace rotos {

// Phone battery + connection, shown on every screen.
struct StatusView {
    bool known;              // have we ever heard from the phone
    bool linkOk;             // packets arriving
    uint8_t battery;         // %
    bool charging;
    uint8_t bars;            // mobile signal 0..4
};

struct HudData {
    ManeuverType next;           // M_NONE when nothing is ahead
    float nextDist;              // metres
    const char* nextRoad;
    ManeuverType then;           // follow-up maneuver if it comes quickly, else M_NONE
    const char* currentRoad;
    int speedKmh;
    uint32_t etaS;               // seconds since midnight
    float remainingM;
    FlyoverHint flyover;         // for the next maneuver
    bool offRoute, linkLost;
    StatusView status;
};

struct DashData {
    bool haveClock;
    uint32_t timeS;              // local time of day
    uint8_t weekday, day, month;
    bool haveWeather;
    Weather weather;
    StatusView status;
};

struct RideSummary {
    float distanceM;
    uint32_t rideS, movingS;
    float avgKmh, maxKmh;
    bool haveClock;
    uint32_t startS, endS;       // local time of day
};

void drawHud(Canvas& cv, const Theme& t, const HudData& d, uint32_t now_ms);
void drawBoot(Canvas& cv, const Theme& t, uint32_t ms);       // ms since power on
void drawWaiting(Canvas& cv, const Theme& t, uint32_t ms);
void drawDashboard(Canvas& cv, const Theme& t, const DashData& d, uint32_t now_ms);
// Drawn over the (frozen) map. secondsLeft: until it returns to the dashboard.
void drawSummary(Canvas& cv, const Theme& t, const RideSummary& r, int secondsLeft, uint32_t now_ms);
// Battery + signal, right-aligned at rightX. withPct adds "78%".
void drawStatus(Canvas& cv, int rightX, int y, const StatusView& s, bool withPct, uint32_t now_ms);

// "240" + "M", "1.2" + "KM", "12" + "KM". value: 16 bytes, unit: 8 bytes
void formatDistance(float m, char* value, char* unit);

} // namespace rotos
