// 4-bit indexed framebuffer (240x320 -> 38,400 bytes) + drawing primitives.
//
// present() expands it to RGB565 through the active palette in 16-row bands
// and only sends bands whose content changed since the last frame, so a
// static screen costs no SPI bandwidth at all.
//
// Glass windows: SNES/GBA-style "colour math" regions. Inside a window, map
// and route pixels are shown through a second, pre-blended palette (dimmed
// toward the panel colour), so panels look frosted without any alpha channel
// or extra framebuffer memory. UI colours (text, icons, borders) stay crisp.
#pragma once
#include <cstdint>
#include "ili9341.h"
#include "theme.h"

namespace rotos {

class Canvas {
public:
    static constexpr int W = SCREEN_W, H = SCREEN_H, STRIDE = W / 2;
    static constexpr int BAND = 16, BANDS = H / BAND;

    uint8_t* data() { return buf_; }
    const uint8_t* data() const { return buf_; }

    inline void pixel(int x, int y, uint8_t c) {
        if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
        uint8_t& b = buf_[y * STRIDE + (x >> 1)];
        b = (x & 1) ? uint8_t((b & 0xF0) | c) : uint8_t((b & 0x0F) | (c << 4));
    }
    inline uint8_t get(int x, int y) const {
        uint8_t b = buf_[y * STRIDE + (x >> 1)];
        return (x & 1) ? (b & 0x0F) : (b >> 4);
    }

    void clear(uint8_t c);
    void hline(int x0, int x1, int y, uint8_t c);
    void fillRect(int x, int y, int w, int h, uint8_t c);
    void rect(int x, int y, int w, int h, uint8_t c);
    void fillCircle(int cx, int cy, int r, uint8_t c);
    void fillTriangle(float x0, float y0, float x1, float y1, float x2, float y2, uint8_t c);
    // Round-capped line of radius r (r = 0 -> 1 px Bresenham).
    void thickLine(float x0, float y0, float x1, float y1, int r, uint8_t c);
    // 50% checkerboard fill: the classic way to get a "semi-transparent"
    // shade with no alpha channel.
    void ditherRect(int x, int y, int w, int h, uint8_t c);

    // Floating panel (Hyprland window / Pokemon dialog box depending on theme).
    // glass = the map stays visible through it, dimmed; otherwise solid.
    void panel(int x, int y, int w, int h, const Theme& t, bool glass = false);

    // Call at the start of every frame: forgets last frame's glass windows.
    void beginFrame() { nGlass_ = 0; }
    // 0 = clear glass, 100 = solid panel colour.
    void setGlassOpacity(int pct);

    void present(Ili9341& lcd, const Theme& t, bool force = false);
    // Row y exactly as the panel receives it (palette + glass applied), in
    // native-endian RGB565. Valid after the first present(). Used by present()
    // itself and by screenshots, so the two can never disagree.
    void rowRgb565(int y, uint16_t* out) const;

private:
    struct Glass { int16_t x, y, w, h; };
    static constexpr int MAX_GLASS = 8;
    // Colours 0..10 (map classes, shadow, route, ridden route) are dimmed
    // under glass; 11..15 (panel, accent, text...) are drawn as they are.
    static constexpr int GLASS_TINTED = C_ROUTE_DONE + 1;
    void glassWindow(int x, int y, int w, int h);
    void buildGlassPalette(const Theme& t);

    Glass glass_[MAX_GLASS];
    int nGlass_ = 0;
    int glassPct_ = 70, builtPct_ = -1;
    uint16_t glassPal_[16];

    uint8_t buf_[W * H / 2] = {};
    uint32_t bandHash_[BANDS] = {};
    uint16_t pal_[16] = {};
    bool havePal_ = false;
};

} // namespace rotos
