#include "mappack.h"
#include <algorithm>
#include <cstring>

namespace rotos {

template <typename T> static T rd(const uint8_t*& p) {
    T v;
    std::memcpy(&v, p, sizeof v);
    p += sizeof v;
    return v;
}

bool MapPack::load(const uint8_t* d, size_t n) {
    const uint8_t* p = d;
    const uint8_t* end = d + n;
    if (n < 36 || std::memcmp(p, "RMAP", 4) != 0) return false;
    p += 4;
    if (rd<uint16_t>(p) != 1) return false;
    zoom = rd<uint8_t>(p);
    tileSize = rd<uint8_t>(p);
    if (tileSize != 32) return false;
    originX = rd<int32_t>(p);
    originY = rd<int32_t>(p);
    tilesW = rd<uint16_t>(p);
    tilesH = rd<uint16_t>(p);
    int slots = rd<uint16_t>(p), nMan = rd<uint16_t>(p);
    uint32_t nPts = rd<uint32_t>(p);
    metresPerPx = rd<float>(p);
    totalM = rd<float>(p);

    size_t idxBytes = size_t(tilesW) * tilesH * 2;
    if (p + idxBytes + size_t(slots) * 512 + nPts * 12 > end) return false;
    // Index is copied so it is 2-byte aligned regardless of the blob.
    indexCopy_.resize(size_t(tilesW) * tilesH);
    std::memcpy(indexCopy_.data(), p, idxBytes);
    index_ = indexCopy_.data();
    p += idxBytes;
    tiles_ = p;
    p += size_t(slots) * 512;

    route.resize(nPts);
    for (auto& r : route) { r.x = rd<float>(p); r.y = rd<float>(p); r.along = rd<float>(p); }
    maneuvers.resize(nMan);
    for (auto& m : maneuvers) {
        if (p + 8 > end) return false;
        m.type = ManeuverType(rd<uint8_t>(p));
        int len = rd<uint8_t>(p);
        m.hint = FlyoverHint(rd<uint8_t>(p));
        p += 1;
        m.along = rd<float>(p);
        if (p + len > end) return false;
        len = std::min(len, 40);
        std::memcpy(m.road, p, len);
        m.road[len] = 0;
        p += len;
    }
    return !route.empty();
}

RoutePoint MapPack::at(float along) const {
    if (along <= route.front().along) return route.front();
    if (along >= route.back().along) return route.back();
    auto it = std::upper_bound(route.begin(), route.end(), along,
                               [](float a, const RoutePoint& r) { return a < r.along; });
    const RoutePoint& b = *it;
    const RoutePoint& a = *(it - 1);
    float t = (along - a.along) / std::max(b.along - a.along, 1e-3f);
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, along};
}

} // namespace rotos
