#include "map_view.h"
#include <cmath>

namespace rotos {

namespace {
struct Basis { float rx, ry, fx, fy; };   // screen-right and forward, in pack px
Basis basis(float h) { float s = std::sin(h), c = std::cos(h); return {c, s, s, -c}; }

inline void toScreen(const Camera& cam, const Basis& b, float wx, float wy, float& sx, float& sy) {
    float dx = wx - cam.x, dy = wy - cam.y;
    sx = cam.ax + dx * b.rx + dy * b.ry;
    sy = cam.ay - (dx * b.fx + dy * b.fy);
}
} // namespace

void drawMap(Canvas& cv, const MapPack& map, const Camera& cam) {
    // Inverse mapping with 16.16 fixed-point steps: per pixel it is two adds
    // and a table lookup, cheap enough for an ESP32-C3 without an FPU.
    Basis b = basis(cam.heading);
    const int32_t ONE = 1 << 16;
    int32_t sxX = int32_t(b.rx * ONE), sxY = int32_t(b.ry * ONE);    // step per screen x
    int32_t syX = int32_t(-b.fx * ONE), syY = int32_t(-b.fy * ONE);  // step per screen y
    float ox = cam.x - cam.ax * b.rx + cam.ay * b.fx;
    float oy = cam.y - cam.ax * b.ry + cam.ay * b.fy;
    int32_t rowX = int32_t(ox * ONE), rowY = int32_t(oy * ONE);

    for (int y = 0; y < Canvas::H; ++y) {
        int32_t wx = rowX, wy = rowY;
        uint8_t* out = cv.data() + y * Canvas::STRIDE;
        for (int x = 0; x < Canvas::W; x += 2) {
            uint8_t px[2];
            for (int k = 0; k < 2; ++k) {
                int ix = wx >> 16, iy = wy >> 16;
                int c = map.classAt(ix, iy);
                // Outside the downloaded corridor: a sparse dot grid, like
                // the void outside a Game Boy map.
                if (c < 0) c = ((ix & 7) == 0 && (iy & 7) == 0) ? C_BUILDING : C_LAND;
                px[k] = uint8_t(c);
                wx += sxX; wy += sxY;
            }
            *out++ = uint8_t((px[0] << 4) | px[1]);
        }
        rowX += syX; rowY += syY;
    }
}

void drawRoute(Canvas& cv, const MapPack& map, const Camera& cam, float along, float nextAlong) {
    Basis b = basis(cam.heading);
    const auto& r = map.route;
    // Project once, split the polyline at the rider.
    static std::vector<float> sx, sy;
    sx.resize(r.size());
    sy.resize(r.size());
    for (size_t i = 0; i < r.size(); ++i) toScreen(cam, b, r[i].x, r[i].y, sx[i], sy[i]);
    RoutePoint here = map.at(along);
    float hx, hy;
    toScreen(cam, b, here.x, here.y, hx, hy);

    auto pass = [&](int radius, uint8_t col, bool done, bool ahead) {
        float px = 0, py = 0;
        bool have = false;
        for (size_t i = 0; i < r.size(); ++i) {
            bool isAhead = r[i].along > along;
            if (have) {
                bool prevAhead = r[i - 1].along > along;
                if (!prevAhead && isAhead) {   // segment containing the rider
                    if (done) cv.thickLine(px, py, hx, hy, radius, col);
                    if (ahead) cv.thickLine(hx, hy, sx[i], sy[i], radius, col);
                } else if ((isAhead && ahead) || (!isAhead && done)) {
                    cv.thickLine(px, py, sx[i], sy[i], radius, col);
                }
            }
            px = sx[i]; py = sy[i]; have = true;
        }
    };
    pass(4, C_SHADOW, false, true);       // casing
    pass(1, C_ROUTE_DONE, true, false);   // ridden: thin and dim
    pass(3, C_ROUTE, false, true);        // ahead: bold

    if (nextAlong > along) {
        RoutePoint m = map.at(nextAlong);
        float mx, my;
        toScreen(cam, b, m.x, m.y, mx, my);
        cv.fillCircle(int(mx), int(my), 5, C_SHADOW);
        cv.fillCircle(int(mx), int(my), 3, C_TEXT);
    }
}

void drawRider(Canvas& cv, const Camera& cam, float riderHeading) {
    // Arrow relative to the (possibly rotated) map.
    float h = riderHeading - cam.heading;
    float fx = std::sin(h), fy = -std::cos(h), rx = -fy, ry = fx;
    float ax = float(cam.ax), ay = float(cam.ay);
    auto arrow = [&](float s, uint8_t col) {
        float tx = ax + fx * 10 * s, ty = ay + fy * 10 * s;
        float lx = ax - fx * 7 * s - rx * 8 * s, ly = ay - fy * 7 * s - ry * 8 * s;
        float qx = ax - fx * 7 * s + rx * 8 * s, qy = ay - fy * 7 * s + ry * 8 * s;
        float nx = ax - fx * 2 * s, ny = ay - fy * 2 * s;
        cv.fillTriangle(tx, ty, lx, ly, nx, ny, col);
        cv.fillTriangle(tx, ty, nx, ny, qx, qy, col);
    };
    arrow(1.45f, C_SHADOW);
    arrow(1.0f, C_HILITE);
}

} // namespace rotos
