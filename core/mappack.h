// Reader for the phone-built map pack (.rmap): quantized map tiles in
// 4-bit map classes + route geometry + maneuvers.
// The blob is parsed in place; on the ESP32 it can live in flash.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "nav.h"

namespace rotos {

struct RoutePoint { float x, y, along; };   // pack pixels, metres

struct Maneuver {
    ManeuverType type;
    FlyoverHint hint;
    float along;
    char road[41];
};

class MapPack {
public:
    bool load(const uint8_t* data, size_t n);

    int zoom = 0, tileSize = 32;
    int32_t originX = 0, originY = 0;      // world px of tile (0,0)
    int tilesW = 0, tilesH = 0;
    float metresPerPx = 1, totalM = 0;
    std::vector<RoutePoint> route;
    std::vector<Maneuver> maneuvers;

    // Map class at pack pixel (x, y); -1 where nothing was stored.
    inline int classAt(int x, int y) const {
        if ((unsigned)x >= (unsigned)(tilesW * 32) || (unsigned)y >= (unsigned)(tilesH * 32)) return -1;
        uint16_t slot = index_[(y >> 5) * tilesW + (x >> 5)];
        if (slot == 0xFFFF) return -1;
        uint8_t b = tiles_[slot * 512 + (y & 31) * 16 + ((x & 31) >> 1)];
        return (x & 1) ? (b & 0x0F) : (b >> 4);
    }

    // Position on the route `along` metres from the start (pack px).
    RoutePoint at(float along) const;

private:
    const uint16_t* index_ = nullptr;
    const uint8_t* tiles_ = nullptr;
    std::vector<uint16_t> indexCopy_;
};

} // namespace rotos
