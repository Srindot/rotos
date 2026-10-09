// Turns 1 Hz, slightly late, sometimes lost phone fixes into smooth 30 fps
// motion along the route: dead-reckon with the last speed, ease toward the
// prediction, and take heading from the route itself (steadier than GPS).
#pragma once
#include <cstdint>
#include "mappack.h"
#include "nav.h"

namespace rotos {

class Tracker {
public:
    void onNavState(const NavState& s, uint32_t now_ms);
    void reset() { have_ = init_ = false; along = speed = 0; }
    void update(uint32_t now_ms, const MapPack& map);

    bool valid() const { return have_; }
    uint32_t sinceFixMs(uint32_t now_ms) const { return have_ ? now_ms - rxMs_ : UINT32_MAX; }
    const NavState& last() const { return last_; }

    float along = 0;      // metres, smoothed
    float heading = 0;    // radians, 0 = north, clockwise
    float speed = 0;      // m/s, smoothed

private:
    NavState last_{};
    uint32_t rxMs_ = 0, prevMs_ = 0;
    bool have_ = false, init_ = false;
};

} // namespace rotos
