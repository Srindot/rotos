// Phone -> ROTOS byte-stream parser. Feed it whatever arrives (Wi-Fi, BLE,
// UART, a capture file) in chunks of any size; it resyncs on garbage.
//
//   | A5 5A | VERSION u8 | TYPE u8 | SEQ u16 | LEN u16 | PAYLOAD | CRC16 |
//   CRC16-CCITT (init 0xFFFF) over VERSION..PAYLOAD, little-endian fields.
#pragma once
#include <cstddef>
#include <cstdint>
#include "nav.h"

namespace rotos {

enum PacketType : uint8_t {
    PKT_NAVIGATION_STATE = 1, PKT_ROUTE_UPDATE, PKT_PHONE_STATUS, PKT_MEDIA_STATE,
    PKT_SETTINGS, PKT_PING, PKT_ACK, PKT_WEATHER,
};

struct Packet {
    uint8_t version, type;
    uint16_t seq, len;
    const uint8_t* payload;
};

class PacketParser {
public:
    static constexpr size_t MAX_PAYLOAD = 512;
    using Handler = void (*)(const Packet&, void* ctx);

    PacketParser(Handler h, void* ctx) : handler_(h), ctx_(ctx) {}
    void feed(const uint8_t* data, size_t n);

    uint32_t ok = 0, crcErrors = 0, dropped = 0;  // link statistics

private:
    enum { SYNC0, SYNC1, HEADER, BODY } st_ = SYNC0;
    uint8_t buf_[6 + MAX_PAYLOAD + 2];
    size_t have_ = 0, need_ = 0;
    int lastSeq_ = -1;
    Handler handler_;
    void* ctx_;
};

uint16_t crc16(const uint8_t* d, size_t n, uint16_t crc = 0xFFFF);
bool decodeNavState(const Packet& p, NavState& out);
bool decodePhoneStatus(const Packet& p, PhoneStatus& out);
bool decodeWeather(const Packet& p, Weather& out);

} // namespace rotos
