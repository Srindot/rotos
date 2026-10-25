"""Python binding of the ROTOS navigation library (core/include/rn/rn_ffi.h).

Thin ctypes wrapper: every function maps 1:1 onto rn_ffi_*; all maths
(map matching, map generation, map pack, protocol) runs in the same C code
the Android app uses through JNI.

    from rotos_nav import Nav
    nav = Nav()            # finds build/host/core/librotos_nav_ffi.so
    nav.reset()
"""
import ctypes as C
import zlib
import os

_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.normpath(os.path.join(_HERE, "..", ".."))
_CANDIDATES = [
    os.environ.get("ROTOS_NAV_LIB", ""),
    os.path.join(_ROOT, "build", "host", "core", "librotos_nav_ffi.so"),
    os.path.join(_ROOT, "build", "core", "librotos_nav_ffi.so"),
]

ROAD_LEN = 41
FRAME_CAP = 300

u8p = C.POINTER(C.c_uint8)
dp = C.POINTER(C.c_double)
fp = C.POINTER(C.c_float)
i32p = C.POINTER(C.c_int32)
u32p = C.POINTER(C.c_uint32)
u16p = C.POINTER(C.c_uint16)

# name: (restype, argtypes)
_SIGNATURES = {
    "rn_ffi_api_version": (C.c_int32, []),
    "rn_ffi_reset": (None, []),
    "rn_ffi_assert_count": (C.c_uint32, []),
    "rn_ffi_route_add_polyline": (C.c_int32, [C.c_char_p]),
    "rn_ffi_route_add_point": (C.c_int32, [C.c_double, C.c_double]),
    "rn_ffi_route_point_count": (C.c_int32, []),
    "rn_ffi_route_add_maneuver": (C.c_int32, [C.c_int32, C.c_int32, C.c_char_p]),
    "rn_ffi_route_add_maneuver_google": (C.c_int32, [C.c_char_p, C.c_int32, C.c_char_p]),
    "rn_ffi_route_finish": (C.c_int32, [C.c_float, C.c_uint32]),
    "rn_ffi_route_total_m": (C.c_float, []),
    "rn_ffi_route_duration_s": (C.c_float, []),
    "rn_ffi_route_maneuver_count": (C.c_int32, []),
    "rn_ffi_route_maneuver": (C.c_int32, [C.c_int32, u8p, u8p, fp, C.c_char_p]),
    "rn_ffi_route_point": (C.c_int32, [C.c_int32, dp, dp, fp]),
    "rn_ffi_bridges_clear": (None, []),
    "rn_ffi_bridges_add": (C.c_int32, [dp, dp, C.c_int32]),
    "rn_ffi_bridges_count": (C.c_int32, []),
    "rn_ffi_flyover_annotate": (C.c_int32, []),
    "rn_ffi_flyover_by_name": (C.c_int32, []),
    "rn_ffi_corridor_plan": (C.c_int32, [C.c_int32, C.c_int32]),
    "rn_ffi_corridor_info": (C.c_int32, [i32p, i32p, i32p, i32p, i32p]),
    "rn_ffi_corridor_keep": (C.c_int32, [C.c_int32, C.c_int32]),
    "rn_ffi_mapgen_block": (C.c_int32, [u8p, C.c_int32, C.c_int32, u8p]),
    "rn_ffi_mapgen_source": (C.c_int32, [C.c_int32]),
    "rn_ffi_pack_begin": (C.c_int32, []),
    "rn_ffi_pack_add_tile": (C.c_int32, [C.c_int32, C.c_int32, u8p]),
    "rn_ffi_pack_finish": (C.c_int32, []),
    "rn_ffi_pack_copy": (C.c_int32, [u8p, C.c_int32]),
    "rn_ffi_wire_set": (C.c_int32, [u8p, C.c_int32]),
    "rn_ffi_wire_len": (C.c_int32, []),
    "rn_ffi_pack_crc32": (C.c_uint32, []),
    "rn_ffi_pack_route_id": (C.c_uint32, []),
    "rn_ffi_pack_load": (C.c_int32, [u8p, C.c_int32]),
    "rn_ffi_pack_class_at": (C.c_int32, [C.c_int32, C.c_int32]),
    "rn_ffi_engine_reset": (None, []),
    "rn_ffi_engine_fix": (C.c_int32, [C.c_uint16, C.c_uint32, C.c_double, C.c_int32, C.c_double, C.c_double,
                                      C.c_float, C.c_float, C.c_float, u8p, C.c_int32]),
    "rn_ffi_engine_state": (C.c_int32, [fp, fp, u32p, u8p, i32p]),
    "rn_ffi_frame_phone_status": (C.c_int32, [C.c_uint16, C.c_uint32, C.c_uint32, C.c_uint8, C.c_uint8,
                                              C.c_uint8, C.c_uint8, C.c_uint8, C.c_uint8, u8p, C.c_int32]),
    "rn_ffi_frame_weather": (C.c_int32, [C.c_uint16, C.c_uint32, C.c_float, C.c_uint8, C.c_uint8, C.c_uint8,
                                         C.c_uint8, u8p, C.c_int32]),
    "rn_ffi_frame_settings": (C.c_int32, [C.c_uint16, C.c_uint8, C.c_uint8, C.c_uint8, C.c_uint8, u8p, C.c_int32]),
    "rn_ffi_frame_ping": (C.c_int32, [C.c_uint16, u8p, C.c_int32]),
    "rn_ffi_frame_forecast": (C.c_int32, [C.c_uint16, C.c_uint32, C.c_uint8, C.c_uint8, C.c_uint16, C.c_uint16,
                                          fp, u8p, u8p, u8p, u8p, C.c_int32]),
    "rn_ffi_frame_call_state": (C.c_int32, [C.c_uint16, C.c_uint32, C.c_uint16, C.c_uint8, C.c_uint8,
                                            C.c_char_p, u8p, C.c_int32]),
    "rn_ffi_frame_prep_status": (C.c_int32, [C.c_uint16, C.c_uint32, C.c_uint8, C.c_uint8, C.c_uint16,
                                             C.c_uint16, u8p, C.c_int32]),
    "rn_ffi_frame_guidance": (C.c_int32, [C.c_uint16, C.c_uint32, C.c_uint8, C.c_uint8, C.c_uint16, C.c_uint32,
                                          C.c_uint32, C.c_char_p, u8p, C.c_int32]),
    "rn_ffi_decode_call_action": (C.c_int32, [u8p, C.c_int32, u16p, u8p]),
    "rn_ffi_frame_notify": (C.c_int32, [C.c_uint16, C.c_uint32, C.c_uint16, C.c_uint8, C.c_char_p, C.c_char_p,
                                        C.c_char_p, u8p, C.c_int32]),
    "rn_ffi_corridor_point_px": (C.c_int32, [C.c_int32, fp, fp]),
    "rn_ffi_frame_map_begin": (C.c_int32, [C.c_uint16, u8p, C.c_int32]),
    "rn_ffi_frame_map_chunk": (C.c_int32, [C.c_uint16, C.c_uint32, u8p, C.c_int32]),
    "rn_ffi_frame_map_end": (C.c_int32, [C.c_uint16, u8p, C.c_int32]),
    "rn_ffi_map_chunk_size": (C.c_int32, []),
    "rn_ffi_rx_reset": (None, []),
    "rn_ffi_rx_feed": (C.c_int32, [u8p, C.c_int32]),
    "rn_ffi_rx_next": (C.c_int32, [u8p, u16p, u8p, C.c_int32]),
    "rn_ffi_decode_ack": (C.c_int32, [u8p, C.c_int32, u8p, u8p, u16p, u32p]),
    "rn_ffi_decode_button": (C.c_int32, [u8p, C.c_int32, u8p, u8p]),
    "rn_ffi_sim_start": (C.c_int32, [C.c_uint32, C.c_float]),
    "rn_ffi_sim_step": (C.c_int32, [C.c_float]),
    "rn_ffi_sim_fix": (C.c_int32, [C.c_float, dp, dp, fp, fp, fp]),
    "rn_ffi_sim_along": (C.c_float, []),
    "rn_ffi_sim_speed": (C.c_float, []),
    "rn_ffi_sim_time": (C.c_float, []),
    "rn_ffi_sun_elevation": (C.c_double, [C.c_double, C.c_double, C.c_double]),
    "rn_ffi_sun_next_sunset": (C.c_double, [C.c_double, C.c_double, C.c_double]),
    "rn_ffi_local_seconds": (C.c_uint32, [C.c_double, C.c_int32]),
}

# Packet types (rn_protocol.h)
NAV_STATE, PHONE_STATUS, WEATHER, SETTINGS, FORECAST, CALL_STATE = 0x01, 0x02, 0x03, 0x04, 0x05, 0x06
PREP_STATUS = 0x07
GUIDANCE = 0x08
NOTIFY = 0x09
NOTIFY_MESSAGE, NOTIFY_OTHER = 0, 1
GUIDE_ACTIVE, GUIDE_ARRIVED = 1, 2
GUIDE_UNKNOWN16, GUIDE_UNKNOWN32 = 0xFFFF, 0xFFFFFFFF
PREP_IDLE, PREP_ROUTE, PREP_BRIDGES, PREP_MAP, PREP_READY, PREP_FAILED = 0, 1, 2, 3, 4, 5
MAP_BEGIN, MAP_CHUNK, MAP_END = 0x10, 0x11, 0x12
PING, ACK, BUTTON, CALL_ACTION = 0x20, 0x21, 0x22, 0x23
CALL_IDLE, CALL_RINGING, CALL_ACTIVE = 0, 1, 2
CALL_F_REPLIED, CALL_F_MISSED = 1, 2
ACTION_REJECT_REPLY, ACTION_DISMISS = 1, 2
ACK_OK = 0
FLAG_GPS_OK, FLAG_OFF_ROUTE, FLAG_REROUTING, FLAG_NIGHT, FLAG_ARRIVED = 1, 2, 4, 8, 16
STATUS_CHARGING, STATUS_NIGHT, STATUS_GOOGLE_CAP = 1, 2, 4
FLYOVER_NAMES = {1: "TAKE FLYOVER", 2: "STAY BELOW", 3: "EXIT FLYOVER"}


class NavError(RuntimeError):
    pass


def _buf(data):
    """bytes/bytearray -> (ctypes pointer, keep-alive object)."""
    arr = (C.c_uint8 * len(data)).from_buffer_copy(bytes(data)) if data else (C.c_uint8 * 1)()
    return C.cast(arr, u8p), arr


class Nav:
    """One navigation session (the C library holds a single static session)."""

    def __init__(self, path=None):
        lib_path = path or next((p for p in _CANDIDATES if p and os.path.exists(p)), None)
        if lib_path is None:
            raise NavError("librotos_nav_ffi.so not found: build it with "
                           "`cmake -S . -B build/host && cmake --build build/host`")
        self.lib = C.CDLL(lib_path)
        for name, (res, args) in _SIGNATURES.items():
            fn = getattr(self.lib, name)
            fn.restype, fn.argtypes = res, args
        if self.lib.rn_ffi_api_version() != 2:
            raise NavError("navigation library API version mismatch")
        self._frame = (C.c_uint8 * FRAME_CAP)()

    # -- helpers ---------------------------------------------------------
    @staticmethod
    def _check(rc, what):
        if rc < 0:
            raise NavError(f"{what} failed: rn_status {rc}")
        return rc

    def _frame_bytes(self, n, what):
        return bytes(self._frame[:self._check(n, what)])

    def _fb(self):
        return C.cast(self._frame, u8p), FRAME_CAP

    # -- session / route -----------------------------------------------------
    def reset(self):
        self.lib.rn_ffi_reset()

    def assert_count(self):
        return self.lib.rn_ffi_assert_count()

    def add_polyline(self, encoded):
        return self._check(self.lib.rn_ffi_route_add_polyline(encoded.encode()), "add_polyline")

    def add_maneuver_google(self, maneuver, point_idx, instruction):
        self._check(self.lib.rn_ffi_route_add_maneuver_google(maneuver.encode(), point_idx,
                                                              (instruction or "").encode("utf-8")),
                    "add_maneuver")

    def finish_route(self, duration_s, route_id):
        self._check(self.lib.rn_ffi_route_finish(duration_s, route_id & 0xFFFFFFFF), "finish_route")

    def total_m(self):
        return self.lib.rn_ffi_route_total_m()

    def point_count(self):
        return self.lib.rn_ffi_route_point_count()

    def point(self, i):
        lat, lon, along = C.c_double(), C.c_double(), C.c_float()
        self._check(self.lib.rn_ffi_route_point(i, C.byref(lat), C.byref(lon), C.byref(along)), "point")
        return lat.value, lon.value, along.value

    def maneuvers(self):
        out = []
        for i in range(self.lib.rn_ffi_route_maneuver_count()):
            t, h, a = C.c_uint8(), C.c_uint8(), C.c_float()
            road = C.create_string_buffer(ROAD_LEN)
            self._check(self.lib.rn_ffi_route_maneuver(i, C.byref(t), C.byref(h), C.byref(a), road), "maneuver")
            out.append({"type": t.value, "hint": h.value, "along_m": a.value, "road": road.value.decode()})
        return out

    # -- flyover ---------------------------------------------------------------
    def bridges_clear(self):
        self.lib.rn_ffi_bridges_clear()

    def bridges_add(self, latlons):
        n = len(latlons)
        lat = (C.c_double * n)(*[p[0] for p in latlons])
        lon = (C.c_double * n)(*[p[1] for p in latlons])
        return self.lib.rn_ffi_bridges_add(lat, lon, n)

    def bridges_count(self):
        return self.lib.rn_ffi_bridges_count()

    def flyover_annotate(self):
        return self._check(self.lib.rn_ffi_flyover_annotate(), "flyover_annotate")

    def flyover_by_name(self):
        return self._check(self.lib.rn_ffi_flyover_by_name(), "flyover_by_name")

    # -- corridor / map generation / pack -------------------------------------
    def corridor_plan(self, zoom, buffer_px):
        return self._check(self.lib.rn_ffi_corridor_plan(zoom, buffer_px), "corridor_plan")

    def corridor_info(self):
        v = [C.c_int32() for _ in range(5)]
        self._check(self.lib.rn_ffi_corridor_info(*[C.byref(x) for x in v]), "corridor_info")
        return dict(zip(("origin_x", "origin_y", "tiles_w", "tiles_h", "n_keep"), (x.value for x in v)))

    def corridor_keep(self, cx, cy):
        return self.lib.rn_ffi_corridor_keep(cx, cy) == 1

    def mapgen_source(self, google):
        """Tiles come from ROTOS's Google style (True) or OSM carto (False)."""
        return self._check(self.lib.rn_ffi_mapgen_source(1 if google else 0), "mapgen_source")

    def mapgen_block(self, rgb, w, h):
        """rgb: bytes of w*h*3 at zoom z+1 -> bytes of (w/2)*(h/2) classes."""
        src, keep = _buf(rgb)
        out = (C.c_uint8 * ((w // 2) * (h // 2)))()
        self._check(self.lib.rn_ffi_mapgen_block(src, w, h, C.cast(out, u8p)), "mapgen_block")
        del keep
        return bytes(out)

    def pack_begin(self):
        self._check(self.lib.rn_ffi_pack_begin(), "pack_begin")

    def pack_add_tile(self, cx, cy, classes):
        src, keep = _buf(classes)
        rc = self.lib.rn_ffi_pack_add_tile(cx, cy, src)
        del keep
        return self._check(rc, "pack_add_tile")

    def pack_finish(self):
        return self._check(self.lib.rn_ffi_pack_finish(), "pack_finish")

    def pack_bytes(self):
        size = 16 * 1024 * 1024
        out = (C.c_uint8 * size)()
        n = self._check(self.lib.rn_ffi_pack_copy(C.cast(out, u8p), size), "pack_copy")
        return bytes(out[:n])

    def compress_wire(self):
        """Compress the current pack for the transfer (raw deflate, 4 KB window,
        like the app); the map frames then carry it. Returns the wire length."""
        z = zlib.compressobj(9, zlib.DEFLATED, -12, 9)
        wire = z.compress(self.pack_bytes()) + z.flush()
        src, keep = _buf(wire)
        rc = self.lib.rn_ffi_wire_set(src, len(wire))
        del keep
        return self._check(rc, "wire_set")

    def wire_len(self):
        """Bytes the map transfer sends."""
        return self._check(self.lib.rn_ffi_wire_len(), "wire_len")

    def pack_load(self, blob):
        src, keep = _buf(blob)
        rc = self.lib.rn_ffi_pack_load(src, len(blob))
        del keep
        return self._check(rc, "pack_load")

    def pack_crc32(self):
        return self.lib.rn_ffi_pack_crc32()

    def pack_route_id(self):
        return self.lib.rn_ffi_pack_route_id()

    def pack_class_at(self, x, y):
        return self.lib.rn_ffi_pack_class_at(x, y)

    # -- engine ------------------------------------------------------------------
    def engine_reset(self):
        self.lib.rn_ffi_engine_reset()

    def engine_fix(self, seq, t_ms, unix_s, utc_offset_s, lat, lon, speed, heading, accuracy):
        n = self.lib.rn_ffi_engine_fix(seq & 0xFFFF, t_ms & 0xFFFFFFFF, unix_s, utc_offset_s, lat, lon,
                                       speed, heading, accuracy, *self._fb())
        return self._frame_bytes(n, "engine_fix")

    def engine_state(self):
        along, rem, rem_s, flags, rr = C.c_float(), C.c_float(), C.c_uint32(), C.c_uint8(), C.c_int32()
        self._check(self.lib.rn_ffi_engine_state(C.byref(along), C.byref(rem), C.byref(rem_s),
                                                 C.byref(flags), C.byref(rr)), "engine_state")
        return {"along_m": along.value, "remaining_m": rem.value, "remaining_s": rem_s.value,
                "flags": flags.value, "wants_reroute": bool(rr.value)}

    # -- frames --------------------------------------------------------------------
    def frame_phone_status(self, seq, t_ms, time_s, weekday, day, month, battery, flags, bars):
        n = self.lib.rn_ffi_frame_phone_status(seq & 0xFFFF, t_ms & 0xFFFFFFFF, time_s, weekday, day, month,
                                               battery, flags, bars, *self._fb())
        return self._frame_bytes(n, "phone_status")

    def frame_weather(self, seq, t_ms, temp_c, code, rain, wind, is_day):
        n = self.lib.rn_ffi_frame_weather(seq & 0xFFFF, t_ms & 0xFFFFFFFF, temp_c, code, rain, wind,
                                          1 if is_day else 0, *self._fb())
        return self._frame_bytes(n, "weather")

    def frame_settings(self, seq, theme, north_up, glass, brightness):
        n = self.lib.rn_ffi_frame_settings(seq & 0xFFFF, theme, north_up, glass, brightness, *self._fb())
        return self._frame_bytes(n, "settings")

    def frame_ping(self, seq):
        return self._frame_bytes(self.lib.rn_ffi_frame_ping(seq & 0xFFFF, *self._fb()), "ping")

    def frame_forecast(self, seq, t_ms, start_hour, sunrise_min, sunset_min, hours):
        """hours: [(temp_c, code, rain_prob, wind_kmh)] (<= 8)."""
        n = min(len(hours), 8)
        temps = (C.c_float * 8)(*[h[0] for h in hours[:n]])
        cols = [(C.c_uint8 * 8)(*[int(h[k]) for h in hours[:n]]) for k in (1, 2, 3)]
        rc = self.lib.rn_ffi_frame_forecast(seq & 0xFFFF, t_ms & 0xFFFFFFFF, start_hour, n, sunrise_min, sunset_min,
                                            temps, *[C.cast(c, u8p) for c in cols], *self._fb())
        return self._frame_bytes(rc, "forecast")

    def frame_call_state(self, seq, t_ms, call_id, state, flags, name):
        rc = self.lib.rn_ffi_frame_call_state(seq & 0xFFFF, t_ms & 0xFFFFFFFF, call_id, state, flags,
                                              (name or "").encode("utf-8"), *self._fb())
        return self._frame_bytes(rc, "call_state")

    def frame_notify(self, seq, t_ms, nid, kind, app, title, text):
        rc = self.lib.rn_ffi_frame_notify(seq & 0xFFFF, t_ms & 0xFFFFFFFF, nid & 0xFFFF, kind,
                                          (app or "").encode("utf-8"), (title or "").encode("utf-8"),
                                          (text or "").encode("utf-8"), *self._fb())
        return self._frame_bytes(rc, "notify")

    def frame_prep_status(self, seq, t_ms, stage, pct, done=0, total=0):
        rc = self.lib.rn_ffi_frame_prep_status(seq & 0xFFFF, t_ms & 0xFFFFFFFF, stage, pct, done, total, *self._fb())
        return self._frame_bytes(rc, "prep_status")

    def frame_guidance(self, seq, t_ms, maneuver, flags, to_turn_m, remaining_m, eta_s, road):
        rc = self.lib.rn_ffi_frame_guidance(seq & 0xFFFF, t_ms & 0xFFFFFFFF, maneuver, flags, to_turn_m,
                                            remaining_m, eta_s, (road or "").encode("utf-8"), *self._fb())
        return self._frame_bytes(rc, "guidance")

    def decode_call_action(self, payload):
        src, keep = _buf(payload)
        cid, act = C.c_uint16(), C.c_uint8()
        rc = self.lib.rn_ffi_decode_call_action(src, len(payload), C.byref(cid), C.byref(act))
        del keep
        self._check(rc, "decode_call_action")
        return cid.value, act.value

    def corridor_point_px(self, i):
        x, y = C.c_float(), C.c_float()
        self._check(self.lib.rn_ffi_corridor_point_px(i, C.byref(x), C.byref(y)), "corridor_point_px")
        return x.value, y.value

    def frame_map_begin(self, seq):
        return self._frame_bytes(self.lib.rn_ffi_frame_map_begin(seq & 0xFFFF, *self._fb()), "map_begin")

    def frame_map_chunk(self, seq, offset):
        n = self.lib.rn_ffi_frame_map_chunk(seq & 0xFFFF, offset, *self._fb())
        return self._frame_bytes(n, "map_chunk") if n > 0 else b""

    def frame_map_end(self, seq):
        return self._frame_bytes(self.lib.rn_ffi_frame_map_end(seq & 0xFFFF, *self._fb()), "map_end")

    def map_chunk_size(self):
        return self.lib.rn_ffi_map_chunk_size()

    # -- inbound -----------------------------------------------------------------------
    def rx_reset(self):
        self.lib.rn_ffi_rx_reset()

    def rx_feed(self, data):
        src, keep = _buf(data)
        n = self.lib.rn_ffi_rx_feed(src, len(data))
        del keep
        return n

    def rx_packets(self):
        """Yields (type, seq, payload bytes) for every queued inbound packet."""
        payload = (C.c_uint8 * 256)()
        t, s = C.c_uint8(), C.c_uint16()
        for _ in range(64):
            n = self.lib.rn_ffi_rx_next(C.byref(t), C.byref(s), C.cast(payload, u8p), 256)
            if n < 0:
                return
            yield t.value, s.value, bytes(payload[:n])

    def decode_ack(self, payload):
        src, keep = _buf(payload)
        at, st, sq, val = C.c_uint8(), C.c_uint8(), C.c_uint16(), C.c_uint32()
        rc = self.lib.rn_ffi_decode_ack(src, len(payload), C.byref(at), C.byref(st), C.byref(sq), C.byref(val))
        del keep
        self._check(rc, "decode_ack")
        return {"type": at.value, "status": st.value, "seq": sq.value, "value": val.value}

    def decode_button(self, payload):
        src, keep = _buf(payload)
        b, e = C.c_uint8(), C.c_uint8()
        rc = self.lib.rn_ffi_decode_button(src, len(payload), C.byref(b), C.byref(e))
        del keep
        self._check(rc, "decode_button")
        return b.value, e.value

    # -- ride simulator ----------------------------------------------------------------------
    def sim_start(self, seed, cruise_mps):
        self._check(self.lib.rn_ffi_sim_start(seed, cruise_mps), "sim_start")

    def sim_step(self, dt):
        return self._check(self.lib.rn_ffi_sim_step(dt), "sim_step") == 1

    def sim_fix(self, sigma_m):
        v = [C.c_double(), C.c_double(), C.c_float(), C.c_float(), C.c_float()]
        self._check(self.lib.rn_ffi_sim_fix(sigma_m, *[C.byref(x) for x in v]), "sim_fix")
        return tuple(x.value for x in v)          # lat, lon, speed, heading, accuracy

    def sim_along(self):
        return self.lib.rn_ffi_sim_along()

    def sim_speed(self):
        return self.lib.rn_ffi_sim_speed()

    # -- misc ------------------------------------------------------------------------------
    def sun_elevation(self, lat, lon, unix_s):
        return self.lib.rn_ffi_sun_elevation(lat, lon, unix_s)

    def next_sunset(self, lat, lon, from_unix_s):
        return self.lib.rn_ffi_sun_next_sunset(lat, lon, from_unix_s)

    def local_seconds(self, unix_s, utc_offset_s):
        return self.lib.rn_ffi_local_seconds(unix_s, utc_offset_s)
