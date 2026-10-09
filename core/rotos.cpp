#include "rotos.h"
#include <algorithm>
#include <cmath>
#include "hud.h"
#include "map_view.h"

namespace rotos {

static constexpr uint32_t BOOT_MS = 2000;
static constexpr uint32_t LINK_TIMEOUT_MS = 3500;
static constexpr uint32_t SUMMARY_MS = 15000;
static constexpr uint32_t THEME_HOLD_MS = 3000;   // flag must hold this long: no flicker
static constexpr float THEME_FADE_S = 1.0f;
static constexpr float MOVING_MPS = 1.5f;         // below this counts as stopped

static RideSummary g_summary;                     // one ride at a time

Rotos::Rotos(Ili9341& lcd, const MapPack& map) : lcd_(lcd), map_(map), parser_(&Rotos::onPacket, this) {}

void Rotos::begin() { lcd_.begin(); }

void Rotos::receive(const uint8_t* bytes, size_t n, uint32_t now_ms) {
    nowMs_ = now_ms;
    parser_.feed(bytes, n);
}

void Rotos::onPacket(const Packet& p, void* ctx) {
    auto* self = static_cast<Rotos*>(ctx);
    self->lastPacketMs_ = self->nowMs_;
    NavState s;
    PhoneStatus ps;
    Weather w;
    if (decodeNavState(p, s)) {
        self->phoneNight_ = s.flags & NAV_NIGHT;
        if (self->state_ == State::Navigating) {
            self->tracker_.onNavState(s, self->nowMs_);
        } else if (self->state_ != State::Summary && s.along_m < self->map_.totalM - 20.f) {
            // A ride on this route begins (or resumes after a restart).
            self->tracker_.reset();
            self->tracker_.onNavState(s, self->nowMs_);
            self->rideRequested_ = true;
        }
    } else if (decodePhoneStatus(p, ps)) {
        self->phone_ = ps;
        self->phoneRxMs_ = self->nowMs_;
        self->havePhone_ = true;
        self->phoneNight_ = ps.flags & STATUS_NIGHT;
    } else if (decodeWeather(p, w)) {
        self->weather_ = w;
        self->haveWeather_ = true;
    }
}

bool Rotos::clockAt(uint32_t now_ms, uint32_t& timeS) const {
    if (!havePhone_) return false;
    timeS = (phone_.time_s + (now_ms - phoneRxMs_) / 1000) % 86400;
    return true;
}

StatusView Rotos::statusView(uint32_t now_ms) const {
    StatusView v{};
    v.known = havePhone_;
    v.linkOk = lastPacketMs_ && now_ms - lastPacketMs_ < LINK_TIMEOUT_MS;
    v.battery = phone_.battery_pct;
    v.charging = phone_.flags & STATUS_CHARGING;
    v.bars = phone_.signal_bars;
    return v;
}

void Rotos::startRide(uint32_t now_ms) {
    rideRequested_ = false;
    rideStartMs_ = now_ms;
    movingMs_ = 0;
    maxSpeed_ = 0;
    startAlong_ = tracker_.last().along_m;
    uint32_t t = 0;
    g_summary = RideSummary{};
    g_summary.haveClock = clockAt(now_ms, t);
    g_summary.startS = t;
    state_ = State::Navigating;
}

void Rotos::finishRide(uint32_t now_ms) {
    RideSummary& r = g_summary;
    r.distanceM = std::max(tracker_.along - startAlong_, 0.f);
    r.rideS = (now_ms - rideStartMs_) / 1000;
    r.movingS = movingMs_ / 1000;
    r.avgKmh = r.movingS ? r.distanceM / r.movingS * 3.6f : 0.f;
    r.maxKmh = maxSpeed_ * 3.6f;
    uint32_t t = 0;
    if (clockAt(now_ms, t)) r.endS = t;
    summaryMs_ = now_ms;
    state_ = State::Summary;
}

void Rotos::updateTheme(uint32_t now_ms) {
    bool want = night_;
    if (mode_ == ThemeMode::Day) want = false;
    else if (mode_ == ThemeMode::Night) want = true;
    else if (havePhone_ || tracker_.valid()) want = phoneNight_;

    if (want != pendingNight_) { pendingNight_ = want; pendingSince_ = now_ms; }
    // Manual modes and the phone's first answer apply at once; later flips must hold.
    bool hold = mode_ == ThemeMode::Auto && phoneSeen_;
    if (mode_ == ThemeMode::Auto && (havePhone_ || tracker_.valid())) phoneSeen_ = true;
    if (pendingNight_ != night_ && (!hold || now_ms - pendingSince_ >= THEME_HOLD_MS)) night_ = pendingNight_;

    float target = night_ ? 1.f : 0.f;
    if (!fadeInit_) { fade_ = target; fadeInit_ = true; }
    float dt = (now_ms - lastTickMs_) * 0.001f;
    lastTickMs_ = now_ms;
    float step = dt / THEME_FADE_S;
    fade_ = fade_ < target ? std::min(fade_ + step, target) : std::max(fade_ - step, target);
    blendThemes(THEME_DAY, THEME_NIGHT, fade_, blended_);
}

void Rotos::tick(uint32_t now_ms) {
    nowMs_ = now_ms;
    updateTheme(now_ms);
    cv_.beginFrame();
    switch (state_) {
    case State::Boot:
        drawBoot(cv_, *theme_, now_ms);
        if (now_ms >= BOOT_MS) state_ = State::Waiting;
        break;
    case State::Waiting:
    case State::Dashboard:
        if (state_ == State::Waiting && (havePhone_ || rideRequested_)) state_ = State::Dashboard;
        if (rideRequested_) { startRide(now_ms); drawNavigation(now_ms); break; }
        if (state_ == State::Waiting) { drawWaiting(cv_, *theme_, now_ms); break; }
        {
            DashData d{};
            d.haveClock = clockAt(now_ms, d.timeS);
            d.weekday = phone_.weekday;
            d.day = phone_.day;
            d.month = phone_.month;
            d.haveWeather = haveWeather_;
            d.weather = weather_;
            d.weather.is_day = weather_.is_day && !phoneNight_;   // after sunset: moon, not sun
            d.status = statusView(now_ms);
            drawDashboard(cv_, *theme_, d, now_ms);
        }
        break;
    case State::Navigating:
        drawNavigation(now_ms);
        break;
    case State::Summary: {
        drawNavigation(now_ms);    // map stays behind the card, frozen at the destination
        uint32_t shown = now_ms - summaryMs_;
        drawSummary(cv_, *theme_, g_summary, int((SUMMARY_MS - std::min(shown, SUMMARY_MS)) / 1000), now_ms);
        if (shown >= SUMMARY_MS) state_ = State::Dashboard;
        break;
    }
    }
    cv_.present(lcd_, *theme_);
}

void Rotos::drawNavigation(uint32_t now_ms) {
    float prevAlong = tracker_.along;
    uint32_t prevMs = lastNavMs_;
    tracker_.update(now_ms, map_);
    lastNavMs_ = now_ms;
    const NavState& s = tracker_.last();
    float along = tracker_.along;

    if (state_ == State::Navigating) {
        if (prevMs && tracker_.speed > MOVING_MPS && along > prevAlong) movingMs_ += now_ms - prevMs;
        maxSpeed_ = std::max(maxSpeed_, tracker_.speed);
        if (map_.totalM - along < 8.f && tracker_.speed < 1.f) finishRide(now_ms);
    }

    // Next announced maneuver, and the one after if it follows quickly.
    const Maneuver* next = nullptr;
    const Maneuver* then = nullptr;
    const char* current = "";
    for (const Maneuver& m : map_.maneuvers) {
        if (m.along <= along + 1.f) { current = m.road; continue; }
        if (!isAnnounced(m.type)) continue;
        if (!next) next = &m;
        else { if (m.along - next->along < 250.f) then = &m; break; }
    }

    // Camera: rider sits low on the screen so most of the view is road ahead.
    RoutePoint here = map_.at(along);
    Camera cam{here.x, here.y, northUp_ ? 0.f : tracker_.heading, 120, northUp_ ? 168 : 212};
    drawMap(cv_, map_, cam);
    drawRoute(cv_, map_, cam, along, next ? next->along : -1.f);
    if (state_ == State::Summary) return;   // the card replaces rider marker and HUD
    drawRider(cv_, cam, tracker_.heading);

    HudData d{};
    d.next = next ? next->type : M_NONE;
    d.nextDist = next ? next->along - along : 0.f;
    d.nextRoad = next ? next->road : "";
    d.flyover = next ? next->hint : FLY_NONE;
    d.then = then ? then->type : M_NONE;
    d.currentRoad = current;
    d.speedKmh = int(std::lround(tracker_.speed * 3.6f));
    d.etaS = s.eta_s;
    d.remainingM = std::max(map_.totalM - along, 0.f);
    d.offRoute = s.flags & NAV_OFF_ROUTE;
    d.linkLost = tracker_.sinceFixMs(now_ms) > LINK_TIMEOUT_MS;
    d.status = statusView(now_ms);
    drawHud(cv_, *theme_, d, now_ms);
}

} // namespace rotos
