#include "protocol.h"
#include <cstring>

namespace rotos {

uint16_t crc16(const uint8_t* d, size_t n, uint16_t crc) {
    while (n--) {
        crc ^= uint16_t(*d++) << 8;
        for (int i = 0; i < 8; ++i) crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}

template <typename T> static T rd(const uint8_t*& p) {
    T v;
    std::memcpy(&v, p, sizeof v);  // wire is little-endian, as are ESP32 and x86
    p += sizeof v;
    return v;
}

void PacketParser::feed(const uint8_t* data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        uint8_t b = data[i];
        switch (st_) {
        case SYNC0: if (b == 0xA5) st_ = SYNC1; break;
        case SYNC1: st_ = (b == 0x5A) ? HEADER : (b == 0xA5 ? SYNC1 : SYNC0); have_ = 0; break;
        case HEADER:
            buf_[have_++] = b;
            if (have_ == 6) {
                uint16_t len = uint16_t(buf_[4] | (buf_[5] << 8));
                if (len > MAX_PAYLOAD) { st_ = SYNC0; ++crcErrors; break; }
                need_ = 6 + len + 2;
                st_ = BODY;
            }
            break;
        case BODY:
            buf_[have_++] = b;
            if (have_ == need_) {
                st_ = SYNC0;
                uint16_t len = uint16_t(need_ - 8);
                uint16_t crc = uint16_t(buf_[need_ - 2] | (buf_[need_ - 1] << 8));
                if (crc16(buf_, 6 + len) != crc) { ++crcErrors; break; }
                Packet p{buf_[0], buf_[1], uint16_t(buf_[2] | (buf_[3] << 8)), len, buf_ + 6};
                // Count gaps going forward only; a late (reordered) packet is not a loss.
                uint16_t gap = uint16_t(p.seq - lastSeq_);
                if (lastSeq_ < 0) lastSeq_ = p.seq;
                else if (gap > 0 && gap < 0x8000) { dropped += gap - 1u; lastSeq_ = p.seq; }
                else if (gap >= 0x8000 && dropped) --dropped;   // late, already counted missing
                ++ok;
                handler_(p, ctx_);
            }
            break;
        }
    }
}

bool decodeNavState(const Packet& p, NavState& s) {
    if (p.type != PKT_NAVIGATION_STATE || p.len < 34) return false;
    const uint8_t* q = p.payload;
    s.t_ms = rd<uint32_t>(q);
    s.lat = rd<int32_t>(q) * 1e-7;
    s.lon = rd<int32_t>(q) * 1e-7;
    s.speed_mps = rd<uint16_t>(q) * 0.01f;
    s.heading_deg = rd<uint16_t>(q) * 0.01f;
    s.along_m = rd<float>(q);
    s.remaining_m = rd<float>(q);
    s.remaining_s = rd<uint32_t>(q);
    s.eta_s = rd<uint32_t>(q);
    s.flags = rd<uint8_t>(q);
    s.accuracy_m = rd<uint8_t>(q);
    return true;
}

bool decodePhoneStatus(const Packet& p, PhoneStatus& s) {
    if (p.type != PKT_PHONE_STATUS || p.len < 14) return false;
    const uint8_t* q = p.payload;
    s.t_ms = rd<uint32_t>(q);
    s.time_s = rd<uint32_t>(q);
    s.weekday = rd<uint8_t>(q);
    s.day = rd<uint8_t>(q);
    s.month = rd<uint8_t>(q);
    s.battery_pct = rd<uint8_t>(q);
    s.flags = rd<uint8_t>(q);
    s.signal_bars = rd<uint8_t>(q);
    return true;
}

bool decodeWeather(const Packet& p, Weather& w) {
    if (p.type != PKT_WEATHER || p.len < 10) return false;
    const uint8_t* q = p.payload;
    w.t_ms = rd<uint32_t>(q);
    w.temp_c = rd<int16_t>(q) * 0.1f;
    w.code = rd<uint8_t>(q);
    w.rain_prob = rd<uint8_t>(q);
    w.wind_kmh = rd<uint8_t>(q);
    w.is_day = rd<uint8_t>(q) != 0;
    return true;
}

} // namespace rotos
