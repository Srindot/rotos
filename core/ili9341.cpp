#include "ili9341.h"

namespace rotos {

void Ili9341::cmd(uint8_t c, std::initializer_list<uint8_t> args) {
    bus_.command(c);
    if (args.size()) bus_.data(args.begin(), args.size());
}

void Ili9341::begin() {
    // Minimal bring-up. Real panels also want power/gamma tuning commands
    // (0xCB, 0xCF, 0xE8, 0xC0, 0xC5, 0xE0...) which the twin ignores.
    cmd(ili::SWRESET);
    cmd(ili::SLPOUT);
    cmd(ili::COLMOD, {0x55});   // 16 bit/pixel
    cmd(ili::MADCTL, {0x48});   // portrait, BGR panel
    cmd(ili::DISPON);
    bus_.flush();
}

void Ili9341::setWindow(int x0, int y0, int x1, int y1) {
    cmd(ili::CASET, {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)});
    cmd(ili::PASET, {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)});
}

} // namespace rotos
