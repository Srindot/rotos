#!/usr/bin/env python3
"""fake_rotos.py - a stand-in ROTOS device on TCP, for testing the Android app.

Speaks the phone <-> ROTOS link protocol v2 (docs/PROTOCOL.md):
  frame  A5 5A | ver=2 | type | seq u16 | len u16 | payload | crc16 u16   (little-endian)
         CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over ver..payload
  map    ACK(MAP_BEGIN), ACK(MAP_CHUNK, value=received) every 16 chunks,
         BAD_STATE for out-of-order chunks, CRC-32 check + ACK at MAP_END
  shows  NAV_STATE / PHONE_STATUS / WEATHER / FORECAST / CALL_STATE as a running summary
  calls  --reply-calls: CALL_ACTION REJECT_REPLY 3 s after a RINGING (button A)
         --dismiss-calls: CALL_ACTION DISMISS instead (button B)

    python3 android/tools/fake_rotos.py                       # listen on 127.0.0.1:7400
    python3 android/tools/fake_rotos.py --drop-chunk 20       # lose chunk #20 once (tests resume)
    python3 android/tools/fake_rotos.py --exit-after-arrived 5 --save out/map.rmap

Standalone: Python 3 standard library only. Exit status 0 when the session
verified (map CRC OK, NAV_STATE seen, ARRIVED seen if --exit-after-arrived).
"""
import argparse
import signal
import socket
import struct
import sys
import time
import zlib

SYNC = b"\xA5\x5A"
VERSION = 2
MAX_PAYLOAD = 248

NAV_STATE, PHONE_STATUS, WEATHER, SETTINGS, FORECAST, CALL_STATE = 0x01, 0x02, 0x03, 0x04, 0x05, 0x06
PREP_STATUS = 0x07
PREP_STAGES = {0: "IDLE", 1: "ROUTE", 2: "BRIDGES", 3: "MAP", 4: "READY", 5: "FAILED"}
MAP_BEGIN, MAP_CHUNK, MAP_END = 0x10, 0x11, 0x12
PING, ACK, BUTTON, CALL_ACTION = 0x20, 0x21, 0x22, 0x23
CALL_STATES = {0: "IDLE", 1: "RINGING", 2: "ACTIVE"}
ACTION_REJECT_REPLY, ACTION_DISMISS = 1, 2
NAMES = {NAV_STATE: "NAV_STATE", PHONE_STATUS: "PHONE_STATUS", WEATHER: "WEATHER",
         SETTINGS: "SETTINGS", MAP_BEGIN: "MAP_BEGIN", MAP_CHUNK: "MAP_CHUNK",
         MAP_END: "MAP_END", PING: "PING", ACK: "ACK", BUTTON: "BUTTON",
         FORECAST: "FORECAST", CALL_STATE: "CALL_STATE", CALL_ACTION: "CALL_ACTION",
         PREP_STATUS: "PREP_STATUS"}
ACK_OK, ACK_BAD_CRC, ACK_TOO_BIG, ACK_BAD_STATE = 0, 1, 2, 3
NAV_FLAGS = [(0x01, "GPS_OK"), (0x02, "OFF_ROUTE"), (0x04, "REROUTING"), (0x08, "NIGHT"),
             (0x10, "ARRIVED")]
MAX_MAP = 4 * 1024 * 1024
ACK_EVERY = 16


def crc16(data, crc=0xFFFF):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame(ptype, seq, payload=b""):
    body = struct.pack("<BBHH", VERSION, ptype, seq & 0xFFFF, len(payload)) + payload
    return SYNC + body + struct.pack("<H", crc16(body))


class Parser:
    """Byte stream -> (type, seq, payload); resyncs on garbage, counts CRC errors."""

    def __init__(self):
        self.buf = bytearray()
        self.crc_errors = 0

    def feed(self, data):
        self.buf += data
        out = []
        while len(self.buf) >= 10:
            i = self.buf.find(SYNC)
            if i < 0:
                del self.buf[:-1]
                break
            if i > 0:
                del self.buf[:i]
            if len(self.buf) < 8:
                break
            ver, ptype, seq, n = struct.unpack_from("<BBHH", self.buf, 2)
            if ver != VERSION or n > MAX_PAYLOAD:
                del self.buf[:1]
                continue
            if len(self.buf) < 8 + n + 2:
                break
            body = bytes(self.buf[2:8 + n])
            (crc,) = struct.unpack_from("<H", self.buf, 8 + n)
            if crc != crc16(body):
                self.crc_errors += 1
                del self.buf[:1]
                continue
            out.append((ptype, seq, body[6:]))
            del self.buf[:8 + n + 2]
        return out


def flags_str(f):
    return "|".join(n for b, n in NAV_FLAGS if f & b) or "-"


class FakeRotos:
    def __init__(self, conn, args):
        self.conn, self.a = conn, args
        self.seq = 0
        self.map = bytearray()
        self.map_len = self.map_crc = self.route_id = self.wire_len = 0
        self.map_active = False
        self.map_ok = None
        self.chunks_since_ack = 0
        self.chunks_seen = 0
        self.dropped = False
        self.map_t0 = 0.0
        self.nav = []          # (wall time, t_ms, along, remaining, flags, route_id)
        self.arrived_at = None
        self.counts = {}
        self.last_seq = None
        self.lost = 0
        self.last_button = time.time()
        self.calls = {}        # call_id -> {"name", "first_ring", "states": [...], "flags", "acted"}
        self.forecasts = 0

    # ---- outbound
    def send(self, ptype, payload=b""):
        self.conn.sendall(frame(ptype, self.seq, payload))
        self.seq = (self.seq + 1) & 0xFFFF

    def ack(self, acked_type, status, acked_seq, value):
        self.send(ACK, struct.pack("<BBHI", acked_type, status, acked_seq, value))

    # ---- inbound
    def on_packet(self, ptype, seq, p):
        self.counts[ptype] = self.counts.get(ptype, 0) + 1
        if self.last_seq is not None and seq != (self.last_seq + 1) & 0xFFFF:
            self.lost += (seq - self.last_seq - 1) & 0xFFFF
        self.last_seq = seq
        handler = {MAP_BEGIN: self.on_map_begin, MAP_CHUNK: self.on_map_chunk,
                   MAP_END: self.on_map_end, NAV_STATE: self.on_nav, PHONE_STATUS: self.on_status,
                   WEATHER: self.on_weather, SETTINGS: self.on_settings, PING: self.on_ping,
                   FORECAST: self.on_forecast, CALL_STATE: self.on_call,
                   PREP_STATUS: self.on_prep}.get(ptype)
        if handler:
            handler(seq, p)
        else:
            print(f"? type 0x{ptype:02x} len {len(p)}")

    def on_map_begin(self, seq, p):
        self.map_len, self.map_crc, self.route_id = struct.unpack("<III", p[:12])
        self.wire_len = struct.unpack("<I", p[12:16])[0] if len(p) >= 16 else 0
        base_crc = struct.unpack("<I", p[16:20])[0] if len(p) >= 20 else 0
        if base_crc:                                  # a patch: the fake keeps no maps to patch
            print(f"MAP_BEGIN as a patch of {base_crc:08x} -> ACK NO_BASE")
            self.ack(MAP_BEGIN, 6, seq, 0)
            return
        status = ACK_OK if 0 < self.map_len <= MAX_MAP else ACK_TOO_BIG
        self.map = bytearray()
        self.map_active = status == ACK_OK
        self.chunks_since_ack = 0
        self.map_t0 = time.time()
        print(f"MAP_BEGIN len={self.map_len} crc={self.map_crc:08x} route_id={self.route_id:08x}"
              f"{f' compressed to {self.wire_len}' if self.wire_len else ''}"
              f" -> ACK {'OK' if status == ACK_OK else 'TOO_BIG'}")
        self.ack(MAP_BEGIN, status, seq, 0)

    def on_map_chunk(self, seq, p):
        offset, n = struct.unpack_from("<IH", p)
        data = p[6:6 + n]
        self.chunks_seen += 1
        if self.a.drop_chunk and self.chunks_seen == self.a.drop_chunk and not self.dropped:
            self.dropped = True
            print(f"  (dropping chunk #{self.chunks_seen} at offset {offset} on purpose)")
            return
        if not self.map_active or offset != len(self.map) or len(data) != n:
            self.ack(MAP_CHUNK, ACK_BAD_STATE, seq, len(self.map))
            return
        self.map += data
        self.chunks_since_ack += 1
        if self.chunks_since_ack >= ACK_EVERY:
            self.chunks_since_ack = 0
            self.ack(MAP_CHUNK, ACK_OK, seq, len(self.map))
            if len(self.map) % (ACK_EVERY * 240 * 32) == 0:
                print(f"  map {len(self.map)}/{self.map_len} B")

    def on_map_end(self, seq, p):
        total, crc = struct.unpack("<II", p[:8])
        wire = len(self.map)
        if self.wire_len:
            if wire != self.wire_len:
                print(f"MAP_END early: have {wire}/{self.wire_len} wire bytes -> ACK BAD_STATE")
                self.ack(MAP_END, ACK_BAD_STATE, seq, wire)
                return
            try:
                self.map = bytearray(zlib.decompressobj(-12).decompress(bytes(self.map)))   # ROTOS: 4 KB window
            except zlib.error as e:
                print(f"MAP_END: inflate failed ({e})")
                self.map = bytearray()
        got = zlib.crc32(bytes(self.map)) & 0xFFFFFFFF
        if len(self.map) != total:
            print(f"MAP_END early: have {len(self.map)}/{total} -> ACK BAD_STATE")
            self.ack(MAP_END, ACK_BAD_STATE, seq, len(self.map))
            return
        ok = got == crc == self.map_crc and total == self.map_len
        self.map_ok = ok
        self.map_active = False
        dt = time.time() - self.map_t0
        print(f"MAP_END len={total} crc={crc:08x} computed={got:08x} -> "
              f"{'OK' if ok else 'BAD_CRC'} ({dt:.1f} s, {wire / max(dt, 1e-3) / 1024:.0f} KiB/s on the link,"
              f" {self.chunks_seen} chunk frames)")
        self.ack(MAP_END, ACK_OK if ok else ACK_BAD_CRC, seq, total)
        if ok and self.a.save:
            with open(self.a.save, "wb") as f:
                f.write(self.map)
            print(f"  saved {self.a.save}")

    def on_nav(self, seq, p):
        (t_ms, rid, lat, lon, spd, hdg, along, rem, rem_s, eta, flags, acc) = \
            struct.unpack("<IIiiHHffIIBB", p[:38])
        now = time.time()
        dt = now - self.nav[-1][0] if self.nav else 0.0
        self.nav.append((now, t_ms, along, rem, flags, rid))
        match = "" if rid == self.route_id else f" (route_id {rid:08x} != map {self.route_id:08x})"
        print(f"NAV #{len(self.nav):4d} seq={seq:5d} dt={dt:4.2f}s {lat / 1e7:.5f},{lon / 1e7:.5f} "
              f"{spd * 0.036:5.1f} km/h hdg {hdg / 100:5.1f} along {along:7.1f} m left {rem:7.1f} m "
              f"{rem_s:4d} s ETA {eta // 3600:02d}:{eta // 60 % 60:02d} acc {acc} m [{flags_str(flags)}]{match}")
        if flags & 0x10 and self.arrived_at is None:
            self.arrived_at = now
            print("*** ARRIVED ***")

    def on_status(self, seq, p):
        t_ms, time_s, wd, day, month, batt, flags, bars = struct.unpack("<IIBBBBBB", p[:14])
        days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
        print(f"PHONE_STATUS {time_s // 3600:02d}:{time_s // 60 % 60:02d}:{time_s % 60:02d} "
              f"{days[wd % 7]} {day:02d}/{month:02d} battery {batt}%{' charging' if flags & 1 else ''}"
              f"{' night' if flags & 2 else ''} signal {bars}/4")

    def on_weather(self, seq, p):
        t_ms, temp, code, rain, wind, is_day = struct.unpack("<IhBBBB", p[:10])
        print(f"WEATHER {temp / 10:.1f} C, WMO {code}, rain {rain}%, wind {wind} km/h, "
              f"{'day' if is_day else 'night'}")

    def on_forecast(self, seq, p):
        t_ms, start, n, rise, sset = struct.unpack_from("<IBBHH", p)
        hours = []
        for i in range(min(n, 8)):
            temp, code, rain, wind = struct.unpack_from("<hBBB", p, 10 + 5 * i)
            hours.append(f"{(start + i) % 24:02d}h {temp / 10:.0f}C w{code} r{rain}% {wind}km/h")
        self.forecasts += 1
        print(f"FORECAST from {start:02d}:00, {n} h, sunrise {rise // 60:02d}:{rise % 60:02d} "
              f"sunset {sset // 60:02d}:{sset % 60:02d} | " + ", ".join(hours))

    def on_prep(self, seq, p):
        t_ms, stage, pct, done, total = struct.unpack_from("<IBBHH", p)
        count = f" {done}/{total}" if total else ""
        print(f"PREP_STATUS {PREP_STAGES.get(stage, stage)} {pct}%{count}")

    def on_call(self, seq, p):
        t_ms, cid, state, flags = struct.unpack_from("<IHBB", p)
        name = p[8:32].split(b"\0")[0].decode("ascii", "replace")
        c = self.calls.setdefault(cid, {"name": name, "first_ring": None, "states": [], "flags": 0,
                                        "acted": False})
        c["name"] = name
        c["flags"] = flags
        if not c["states"] or c["states"][-1] != state:
            c["states"].append(state)
        if state == 1 and c["first_ring"] is None:
            c["first_ring"] = time.time()
        fl = "|".join(n for b, n in ((1, "REPLIED"), (2, "MISSED")) if flags & b) or "-"
        print(f"CALL_STATE id={cid} {CALL_STATES.get(state, state)} \"{name}\" flags={fl}")

    def maybe_call_action(self):
        action = ACTION_REJECT_REPLY if self.a.reply_calls else ACTION_DISMISS if self.a.dismiss_calls else 0
        if not action:
            return
        for cid, c in self.calls.items():
            ringing = c["states"] and c["states"][-1] == 1
            if ringing and not c["acted"] and time.time() - c["first_ring"] >= 3.0:
                c["acted"] = True
                self.send(CALL_ACTION, struct.pack("<HBB", cid, action, 0))
                print(f"-> CALL_ACTION id={cid} {'REJECT_REPLY' if action == 1 else 'DISMISS'}")

    def on_settings(self, seq, p):
        print(f"SETTINGS {p[:4].hex()}")

    def on_ping(self, seq, p):
        if self.a.verbose:
            print("PING")

    def maybe_button(self):
        if self.a.button_every and time.time() - self.last_button >= self.a.button_every:
            self.last_button = time.time()
            self.send(BUTTON, bytes([0, 1]))
            print("-> BUTTON 0 short")

    # ---- summary
    def summary(self):
        print("\n==== fake ROTOS summary ====")
        print("packets: " + ", ".join(f"{NAMES.get(k, hex(k))} {v}" for k, v in sorted(self.counts.items())))
        print(f"sequence gaps (phone->ROTOS): {self.lost}")
        print(f"map: {'CRC OK' if self.map_ok else 'not verified'} ({len(self.map)} B, route_id {self.route_id:08x})")
        ok = bool(self.map_ok)
        if len(self.nav) >= 2:
            dts = [b[0] - a[0] for a, b in zip(self.nav, self.nav[1:])]
            dtms = [((b[1] - a[1]) & 0xFFFFFFFF) for a, b in zip(self.nav, self.nav[1:])]
            alongs = [n[2] for n in self.nav]
            back = sum(1 for a, b in zip(alongs, alongs[1:]) if b < a - 0.5)
            print(f"NAV_STATE: {len(self.nav)} packets, wall interval mean {sum(dts) / len(dts):.2f} s "
                  f"(min {min(dts):.2f}, max {max(dts):.2f}), t_ms step mean {sum(dtms) / len(dtms):.0f} ms")
            print(f"along: {alongs[0]:.1f} -> {alongs[-1]:.1f} m, steps backwards: {back}, "
                  f"route_id match: {all(n[5] == self.route_id for n in self.nav)}")
        else:
            ok = False
            print("NAV_STATE: none")
        print(f"ARRIVED: {'yes' if self.arrived_at else 'no'}")
        print(f"FORECAST packets: {self.forecasts}")
        for cid, c in self.calls.items():
            fl = "|".join(n for b, n in ((1, "REPLIED"), (2, "MISSED")) if c["flags"] & b) or "-"
            print(f"call {cid} \"{c['name']}\": " + " -> ".join(CALL_STATES.get(x, str(x)) for x in c["states"])
                  + f" (final flags {fl})")
        if self.a.exit_after_arrived is not None and not self.arrived_at:
            ok = False
        print(f"RESULT: {'PASS' if ok else 'FAIL'}")
        return ok


def serve(args):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.host, args.port))
    srv.listen(1)
    print(f"fake ROTOS listening on {args.host}:{args.port}")
    srv.settimeout(args.accept_timeout)
    try:
        conn, peer = srv.accept()
    except socket.timeout:
        print("no phone connected")
        return False
    print(f"phone connected from {peer[0]}:{peer[1]}")
    conn.settimeout(0.2)
    rotos, parser = FakeRotos(conn, args), Parser()
    try:
        while True:
            try:
                data = conn.recv(65536)
                if not data:
                    print("phone disconnected")
                    break
                for ptype, seq, payload in parser.feed(data):
                    rotos.on_packet(ptype, seq, payload)
            except socket.timeout:
                pass
            rotos.maybe_button()
            rotos.maybe_call_action()
            if (args.exit_after_arrived is not None and rotos.arrived_at
                    and time.time() - rotos.arrived_at >= args.exit_after_arrived):
                break
    except KeyboardInterrupt:
        pass
    finally:
        conn.close()
        srv.close()
    if parser.crc_errors:
        print(f"frame CRC errors: {parser.crc_errors}")
    return rotos.summary()


def _stop(signum, frame):
    raise KeyboardInterrupt      # SIGTERM ends the session like Ctrl-C (summary is printed)


def main():
    signal.signal(signal.SIGTERM, _stop)
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7400)
    ap.add_argument("--drop-chunk", type=int, default=0, help="ignore the Nth MAP_CHUNK once")
    ap.add_argument("--save", help="write the received map pack here")
    ap.add_argument("--button-every", type=float, default=0, help="send a BUTTON event every S seconds")
    ap.add_argument("--exit-after-arrived", type=float, default=None,
                    help="stop S seconds after the first ARRIVED flag")
    ap.add_argument("--accept-timeout", type=float, default=None)
    ap.add_argument("--reply-calls", action="store_true", help="REJECT_REPLY 3 s after RINGING")
    ap.add_argument("--dismiss-calls", action="store_true", help="DISMISS 3 s after RINGING")
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()
    sys.stdout.reconfigure(line_buffering=True)
    sys.exit(0 if serve(a) else 1)


if __name__ == "__main__":
    main()
