// ILI9341 driver: 240x320 TFT, RGB565, portrait.
// Written against SpiBus only, so it runs unchanged on the twin and the ESP32.
#pragma once
#include <initializer_list>
#include "spi_bus.h"

namespace rotos {

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;

namespace ili {
constexpr uint8_t SWRESET = 0x01, SLPIN = 0x10, SLPOUT = 0x11, DISPOFF = 0x28, DISPON = 0x29,
                  CASET = 0x2A, PASET = 0x2B, RAMWR = 0x2C, MADCTL = 0x36, COLMOD = 0x3A;
}

class Ili9341 {
public:
    explicit Ili9341(SpiBus& bus) : bus_(bus) {}

    void begin();
    void setWindow(int x0, int y0, int x1, int y1);   // inclusive
    // Stream big-endian RGB565 bytes into the current window.
    void writePixels(const uint8_t* be565, size_t bytes) { bus_.data(be565, bytes); }
    void startWrite() { bus_.command(ili::RAMWR); }
    void flush() { bus_.flush(); }

private:
    void cmd(uint8_t c, std::initializer_list<uint8_t> args = {});
    SpiBus& bus_;
};

} // namespace rotos
