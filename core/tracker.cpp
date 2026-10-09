#include "tracker.h"
#include <algorithm>
#include <cmath>

namespace rotos {

static constexpr float PI = 3.14159265f;

void Tracker::onNavState(const NavState& s, uint32_t now_ms) {
    last_ = s;
    rxMs_ = now_ms;
    have_ = true;
}

void Tracker::update(uint32_t now_ms, const MapPack& map) {
    if (!have_) return;
    float dt = init_ ? std::min((now_ms - prevMs_) * 0.001f, 0.25f) : 0.f;
    prevMs_ = now_ms;

    // Dead reckoning, but never more than 2 s past the last fix.
    float age = std::min((now_ms - rxMs_) * 0.001f, 2.0f);
    float pred = std::min(last_.along_m + last_.speed_mps * age, map.totalM);

    if (!init_) {
        along = pred;
        speed = last_.speed_mps;
        init_ = true;
    } else {
        float diff = pred - along;
        if (std::fabs(diff) > 60.f) along = pred;                       // reroute / jump
        else if (diff > 0 || diff < -15.f) along += diff * (1.f - std::exp(-dt / 0.3f));
        // small backward corrections are ignored: the marker never jitters backwards
        speed += (last_.speed_mps - speed) * (1.f - std::exp(-dt / 0.5f));
    }

    // Heading from the route ahead of us.
    RoutePoint a = map.at(along - 5.f), b = map.at(along + 25.f);
    float dx = b.x - a.x, dy = b.y - a.y;
    if (dx * dx + dy * dy > 4.f) {
        float target = std::atan2(dx, -dy);
        if (dt == 0.f) heading = target;
        float d = std::remainder(target - heading, 2 * PI);
        heading = std::remainder(heading + d * (1.f - std::exp(-dt / 0.35f)), 2 * PI);
    }
}

} // namespace rotos
