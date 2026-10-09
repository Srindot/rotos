// Draws the map: rotated raster of quantized map classes, the route on top
// (ridden part dimmed), the next maneuver point and the rider marker.
#pragma once
#include "canvas.h"
#include "mappack.h"

namespace rotos {

struct Camera {
    float x, y;          // pack px under the anchor
    float heading;       // radians; the map is rotated so this points up
    int ax, ay;          // anchor on screen (where the rider is)
};

void drawMap(Canvas& cv, const MapPack& map, const Camera& cam);
void drawRoute(Canvas& cv, const MapPack& map, const Camera& cam, float along, float nextManeuverAlong);
void drawRider(Canvas& cv, const Camera& cam, float riderHeading);

} // namespace rotos
