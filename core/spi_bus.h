// The one seam between ROTOS software and hardware.
//
// The ILI9341 is driven over SPI with a D/C (data/command) pin. Anything that
// can send "a command byte" and "some data bytes" can drive the display:
//   - host/tcp_spi_bus.cpp : sends them over TCP to the screen digital twin
//   - (later) ESP32 SPI peripheral with DMA
#pragma once
#include <cstddef>
#include <cstdint>

namespace rotos {

class SpiBus {
public:
    virtual ~SpiBus() = default;
    virtual void command(uint8_t cmd) = 0;                  // D/C low
    virtual void data(const uint8_t* bytes, size_t n) = 0;  // D/C high
    virtual void flush() {}
};

} // namespace rotos
