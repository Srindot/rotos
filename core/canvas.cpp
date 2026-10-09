#include "canvas.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace rotos {

void Canvas::clear(uint8_t c) { std::memset(buf_, (c << 4) | c, sizeof buf_); }

void Canvas::hline(int x0, int x1, int y, uint8_t c) {
    if ((unsigned)y >= (unsigned)H) return;
    if (x0 > x1) std::swap(x0, x1);
    x0 = std::max(x0, 0);
    x1 = std::min(x1, W - 1);
    if (x0 > x1) return;
    if (x0 & 1) pixel(x0++, y, c);
    if (!(x1 & 1)) pixel(x1--, y, c);
    if (x0 < x1) std::memset(&buf_[y * STRIDE + (x0 >> 1)], (c << 4) | c, (x1 - x0 + 1) >> 1);
}

void Canvas::fillRect(int x, int y, int w, int h, uint8_t c) {
    for (int j = std::max(y, 0); j < std::min(y + h, H); ++j) hline(x, x + w - 1, j, c);
}

void Canvas::rect(int x, int y, int w, int h, uint8_t c) {
    hline(x, x + w - 1, y, c);
    hline(x, x + w - 1, y + h - 1, c);
    for (int j = y + 1; j < y + h - 1; ++j) { pixel(x, j, c); pixel(x + w - 1, j, c); }
}

void Canvas::ditherRect(int x, int y, int w, int h, uint8_t c) {
    for (int j = std::max(y, 0); j < std::min(y + h, H); ++j)
        for (int i = x + ((x + j) & 1); i < x + w; i += 2) pixel(i, j, c);
}

void Canvas::fillCircle(int cx, int cy, int r, uint8_t c) {
    for (int dy = -r; dy <= r; ++dy) {
        int dx = int(std::sqrt(float(r * r - dy * dy)) + 0.35f);
        hline(cx - dx, cx + dx, cy + dy, c);
    }
}

void Canvas::fillTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint8_t c) {
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    int ya = std::max(int(std::ceil(y0)), 0), yb = std::min(int(std::floor(y2)), H - 1);
    for (int y = ya; y <= yb; ++y) {
        float fy = float(y);
        float xa = (y2 != y0) ? x0 + (x2 - x0) * (fy - y0) / (y2 - y0) : x0;
        float xb;
        if (fy < y1) xb = (y1 != y0) ? x0 + (x1 - x0) * (fy - y0) / (y1 - y0) : x1;
        else         xb = (y2 != y1) ? x1 + (x2 - x1) * (fy - y1) / (y2 - y1) : x1;
        if (xa > xb) std::swap(xa, xb);
        hline(int(std::lround(xa)), int(std::lround(xb)), y, c);
    }
}

void Canvas::thickLine(float fx0, float fy0, float fx1, float fy1, int r, uint8_t c) {
    // Cheap reject: segment bbox entirely off screen.
    if (std::max(fx0, fx1) < -r || std::min(fx0, fx1) > W + r ||
        std::max(fy0, fy1) < -r || std::min(fy0, fy1) > H + r) return;
    int x0 = int(std::lround(fx0)), y0 = int(std::lround(fy0));
    int x1 = int(std::lround(fx1)), y1 = int(std::lround(fy1));
    // Clamp absurd lengths (points far behind the camera) to keep loops bounded.
    if (std::abs(x1 - x0) > 4000 || std::abs(y1 - y0) > 4000) return;
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    // Stamp a disc at each step. Fine for r <= 4 at this resolution.
    for (;;) {
        if (r == 0) pixel(x0, y0, c);
        else if (x0 >= -r && x0 < W + r && y0 >= -r && y0 < H + r) fillCircle(x0, y0, r, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static const uint8_t BAYER4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

void Canvas::glassWindow(int x, int y, int w, int h) {
    if (nGlass_ < MAX_GLASS) glass_[nGlass_++] = {int16_t(x), int16_t(y), int16_t(w), int16_t(h)};
}

void Canvas::setGlassOpacity(int pct) { glassPct_ = pct < 0 ? 0 : (pct > 100 ? 100 : pct); }

void Canvas::buildGlassPalette(const Theme& t) {
    auto ex = [](uint16_t c, int& r, int& g, int& b) {
        r = (c >> 11) << 3 | (c >> 13); g = ((c >> 5) & 63) << 2 | ((c >> 9) & 3); b = (c & 31) << 3 | ((c >> 2) & 7);
    };
    int pr, pg, pb;
    ex(t.rgb565[C_PANEL], pr, pg, pb);
    for (int i = 0; i < 16; ++i) {
        if (i >= GLASS_TINTED) { glassPal_[i] = t.rgb565[i]; continue; }
        int r, g, b;
        ex(t.rgb565[i], r, g, b);
        r += (pr - r) * glassPct_ / 100; g += (pg - g) * glassPct_ / 100; b += (pb - b) * glassPct_ / 100;
        glassPal_[i] = uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
    builtPct_ = glassPct_;
}

void Canvas::panel(int x, int y, int w, int h, const Theme& t, bool glass) {
    if (glass) {
        // Inside the border: the map shows through, dimmed at present() time.
        glassWindow(x + 1, y + 1, w - 2, h - 2);
    } else {
        // Body with 2 px rounded corners.
        fillRect(x + 2, y, w - 4, h, C_PANEL);
        fillRect(x, y + 2, w, h - 4, C_PANEL);
        fillRect(x + 1, y + 1, w - 2, h - 2, C_PANEL);
    }

    auto border = [&](int i, int j, uint8_t inner) {
        if (t.panel == PanelStyle::Gradient) {
            // ACCENT -> HILITE along the diagonal, ordered-dithered with 2 colours.
            int lvl = (i - x + j - y) * 16 / (w + h);
            pixel(i, j, lvl > BAYER4[j & 3][i & 3] ? C_HILITE : C_ACCENT);
        } else {
            pixel(i, j, inner);
        }
    };
    auto outline = [&](int ox, int oy, int ow, int oh, uint8_t col) {
        for (int i = ox + 2; i < ox + ow - 2; ++i) { border(i, oy, col); border(i, oy + oh - 1, col); }
        for (int j = oy + 2; j < oy + oh - 2; ++j) { border(ox, j, col); border(ox + ow - 1, j, col); }
        border(ox + 1, oy + 1, col); border(ox + ow - 2, oy + 1, col);
        border(ox + 1, oy + oh - 2, col); border(ox + ow - 2, oy + oh - 2, col);
    };
    if (t.panel == PanelStyle::Gradient) {
        outline(x, y, w, h, C_ACCENT);
    } else {
        outline(x, y, w, h, C_SHADOW);
        outline(x + 2, y + 2, w - 4, h - 4, C_ACCENT);
    }
}

void Canvas::rowRgb565(int y, uint16_t* out) const {
    const uint16_t* pal = havePal_ ? pal_ : THEME_NIGHT.rgb565;
    const uint8_t* src = &buf_[y * STRIDE];
    for (int i = 0; i < STRIDE; ++i) {
        out[2 * i] = pal[src[i] >> 4];
        out[2 * i + 1] = pal[src[i] & 0x0F];
    }
    // Pixels under glass use the dimmed palette.
    for (int g = 0; g < nGlass_; ++g) {
        const Glass& w = glass_[g];
        if (y < w.y || y >= w.y + w.h) continue;
        int edge = std::min(y - w.y, w.y + w.h - 1 - y);
        int inset = edge == 0 ? 1 : 0;                 // follow the rounded corners
        int xa = std::max(w.x + inset, 0), xb = std::min(w.x + w.w - 1 - inset, W - 1);
        for (int x = xa; x <= xb; ++x) out[x] = glassPal_[get(x, y)];
    }
}

void Canvas::present(Ili9341& lcd, const Theme& t, bool force) {
    if (!havePal_ || std::memcmp(pal_, t.rgb565, sizeof pal_) != 0 || builtPct_ != glassPct_) {
        force = true;                     // new palette: every band must be resent
        std::memcpy(pal_, t.rgb565, sizeof pal_);
        havePal_ = true;
        buildGlassPalette(t);
    }
    // 16 rows of big-endian RGB565 = 7.5 KiB: a comfortable DMA buffer on an ESP32.
    static uint8_t line[W * BAND * 2];
    for (int b = 0; b < BANDS; ++b) {
        const uint8_t* src = &buf_[b * BAND * STRIDE];
        uint32_t h = 2166136261u;                       // FNV-1a
        for (int i = 0; i < BAND * STRIDE; ++i) h = (h ^ src[i]) * 16777619u;
        int y0 = b * BAND;
        for (int g = 0; g < nGlass_; ++g) {          // glass placement is part of the look
            const Glass& w = glass_[g];
            if (w.y < y0 + BAND && w.y + w.h > y0)
                h = (h ^ uint32_t(w.x | w.y << 9 | w.w << 18)) * 16777619u ^ uint32_t(w.h);
        }
        if (!force && h == bandHash_[b]) continue;
        bandHash_[b] = h;

        uint16_t row[W];
        uint8_t* o = line;
        for (int y = y0; y < y0 + BAND; ++y) {
            rowRgb565(y, row);
            for (int x = 0; x < W; ++x) { *o++ = uint8_t(row[x] >> 8); *o++ = uint8_t(row[x]); }  // big-endian on the wire
        }
        lcd.setWindow(0, b * BAND, W - 1, b * BAND + BAND - 1);
        lcd.startWrite();
        lcd.writePixels(line, sizeof line);
    }
    lcd.flush();
}

} // namespace rotos
