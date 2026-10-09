// ROTOS application: state machine + glue between link, tracker and renderers.
// Platform-free: the host (desktop twin or ESP32) supplies bytes, a clock
// and an Ili9341; ROTOS does the rest.
//
//   Boot ─► Waiting ─(phone heard)─► Dashboard ─(route starts)─► Navigating
//                                       ▲                            │ arrived
//                                       └──── 15 s ──── Summary ◄────┘
#pragma once
#include "canvas.h"
#include "hud.h"
#include "mappack.h"
#include "protocol.h"
#include "tracker.h"

namespace rotos {

class Rotos {
public:
    enum class State { Boot, Waiting, Dashboard, Navigating, Summary };
    // Auto follows the phone's NIGHT flag (sun below the horizon).
    enum class ThemeMode { Auto, Day, Night };

    Rotos(Ili9341& lcd, const MapPack& map);

    void setThemeMode(ThemeMode m) { mode_ = m; }
    void setNorthUp(bool on) { northUp_ = on; }
    void setGlassOpacity(int pct) { cv_.setGlassOpacity(pct); }

    void begin();
    void receive(const uint8_t* bytes, size_t n, uint32_t now_ms);   // from the phone link
    void tick(uint32_t now_ms);                                      // update + draw + present

    State state() const { return state_; }
    bool isNight() const { return night_; }
    const Canvas& canvas() const { return cv_; }
    const PacketParser& link() const { return parser_; }

private:
    static void onPacket(const Packet& p, void* self);
    void drawNavigation(uint32_t now_ms);
    void updateTheme(uint32_t now_ms);
    void startRide(uint32_t now_ms);
    void finishRide(uint32_t now_ms);
    bool clockAt(uint32_t now_ms, uint32_t& timeS) const;
    StatusView statusView(uint32_t now_ms) const;

    Ili9341& lcd_;
    const MapPack& map_;
    Canvas cv_;
    PacketParser parser_;
    Tracker tracker_;
    State state_ = State::Boot;
    bool northUp_ = false;
    uint32_t nowMs_ = 0;

    // Phone link
    uint32_t lastPacketMs_ = 0;
    bool havePhone_ = false, haveWeather_ = false, rideRequested_ = false;
    PhoneStatus phone_{};
    uint32_t phoneRxMs_ = 0;
    Weather weather_{};
    bool phoneNight_ = true;

    // Ride statistics
    uint32_t rideStartMs_ = 0, movingMs_ = 0, summaryMs_ = 0;
    float startAlong_ = 0, maxSpeed_ = 0;
    uint32_t lastNavMs_ = 0;

    // Theme
    ThemeMode mode_ = ThemeMode::Auto;
    bool night_ = true;               // what we are showing (or fading to)
    bool pendingNight_ = true;        // what the phone asks for
    uint32_t pendingSince_ = 0;
    float fade_ = 1.f;                // 0 = day palette, 1 = night palette
    bool fadeInit_ = false;
    bool phoneSeen_ = false;          // first phone decision applies without the hold
    uint32_t lastTickMs_ = 0;
    Theme blended_ = THEME_NIGHT;
    const Theme* theme_ = &blended_;
};

} // namespace rotos
