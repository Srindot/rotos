// SpiBus implementation that drives the rotos_screen twin over TCP.
// On the ESP32 this file is replaced by one using the SPI peripheral.
#pragma once
#include <string>
#include <vector>
#include "spi_bus.h"

class TcpSpiBus : public rotos::SpiBus {
public:
    bool connect(const std::string& host, int port, int timeoutMs);
    void command(uint8_t cmd) override;
    void data(const uint8_t* bytes, size_t n) override;
    void flush() override;
    bool ok() const { return fd_ >= 0; }

private:
    void frame(uint8_t kind, const uint8_t* bytes, size_t n);
    int fd_ = -1;
    std::vector<uint8_t> out_;
};

// Counts bytes instead of sending them: headless runs and bandwidth stats.
class NullSpiBus : public rotos::SpiBus {
public:
    void command(uint8_t) override { bytes += 1; }
    void data(const uint8_t*, size_t n) override { bytes += n; }
    uint64_t bytes = 0;
};
