// The screen twin's only interface: a virtual SPI link over TCP.
//
// Each SPI transfer becomes one frame:   [kind u8][len u32 LE][len bytes]
//   kind 0 = command byte (D/C pin low), len == 1
//   kind 1 = data bytes   (D/C pin high)
// The bytes are exactly what would go down the wire to a real ILI9341.
#pragma once
#include <cstdint>

namespace screen_wire {
constexpr uint16_t DEFAULT_PORT = 9341;
constexpr uint8_t FRAME_CMD = 0, FRAME_DATA = 1;
} // namespace screen_wire
