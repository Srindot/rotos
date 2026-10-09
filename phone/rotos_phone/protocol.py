"""Phone -> ROTOS wire protocol (see core/protocol.h).

  | SYNC A5 5A | VERSION u8 | TYPE u8 | SEQ u16 | LEN u16 | PAYLOAD | CRC16 |

CRC16-CCITT (0xFFFF init) over VERSION..PAYLOAD. Little-endian.
"""
import struct

VERSION = 1
NAVIGATION_STATE, ROUTE_UPDATE, PHONE_STATUS, MEDIA_STATE, SETTINGS, PING, ACK, WEATHER = range(1, 9)

FLAG_GPS_OK, FLAG_OFF_ROUTE, FLAG_REROUTING, FLAG_NIGHT = 1, 2, 4, 8
STATUS_CHARGING, STATUS_NIGHT = 1, 2


def crc16(data, crc=0xFFFF):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else (crc << 1)
            crc &= 0xFFFF
    return crc


def packet(ptype, seq, payload):
    body = struct.pack("<BBHH", VERSION, ptype, seq & 0xFFFF, len(payload)) + payload
    return b"\xA5\x5A" + body + struct.pack("<H", crc16(body))


def nav_state(t_ms, lat, lon, speed_mps, heading_deg, along_m, remaining_m,
              remaining_s, eta_s_of_day, flags, accuracy_m):
    return struct.pack("<IiiHHffIIBB", t_ms, round(lat * 1e7), round(lon * 1e7),
                       min(int(speed_mps * 100), 0xFFFF), int(heading_deg * 100) % 36000,
                       along_m, remaining_m, int(remaining_s), int(eta_s_of_day) % 86400,
                       flags, min(int(accuracy_m), 255))


def phone_status(t_ms, local_time, battery_pct, charging, signal_bars, night):
    """Clock, battery, mobile signal (0..4 bars) and day/night, every few seconds.
    local_time: datetime in the phone's timezone."""
    flags = (STATUS_CHARGING if charging else 0) | (STATUS_NIGHT if night else 0)
    secs = local_time.hour * 3600 + local_time.minute * 60 + local_time.second
    return struct.pack("<IIBBBBBB", t_ms, secs, local_time.weekday(), local_time.day, local_time.month,
                       battery_pct, flags, signal_bars)


def weather(t_ms, temp_c, wmo_code, rain_prob_pct, wind_kmh, is_day):
    """Current conditions (WMO weather code, as used by Open-Meteo)."""
    return struct.pack("<IhBBBB", t_ms, round(temp_c * 10), wmo_code, rain_prob_pct,
                       min(int(wind_kmh), 255), 1 if is_day else 0)


def capture(events, rnd, loss=0.03):
    """events: [(t_ms, type, payload)] -> .rfeed bytes.
    One sequence counter for the link; Wi-Fi latency 80-200 ms; ~3% loss."""
    out = []
    for seq, (t, ptype, payload) in enumerate(sorted(events, key=lambda e: e[0])):
        if seq > 0 and rnd.random() < loss:
            continue
        out.append((t + 80 + rnd.randint(0, 120), packet(ptype, seq, payload)))
    out.sort(key=lambda r: r[0])
    return b"".join(capture_record(d, p) for d, p in out)


def capture_record(deliver_ms, pkt):
    """.rfeed = a timestamped capture of wire bytes, like a tiny pcap."""
    return struct.pack("<IH", deliver_ms, len(pkt)) + pkt
