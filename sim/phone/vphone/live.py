"""Live mode: streams a session to `rotos_sim --listen PORT` over TCP in real
time - the same conversation the Android app has with ROTOS over BLE,
including the windowed map upload with ACKs and the call popup round trip
(docs/PROTOCOL.md).
"""
import socket
import time

import rotos_nav as rn

from . import timeline

WINDOW_CHUNKS = 16


class Link:
    def __init__(self, nav, host, port):
        self.nav = nav
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.settimeout(0.05)
        self.seq = 0
        self.acks = []
        self.actions = []
        nav.rx_reset()

    def send(self, build):
        self.sock.sendall(build(self.seq))
        self.seq = (self.seq + 1) & 0xFFFF

    def poll(self):
        """Reads what ROTOS sent; ACKs are queued, everything else handled."""
        try:
            data = self.sock.recv(4096)
        except socket.timeout:
            return
        if not data:
            raise ConnectionError("ROTOS closed the link")
        self.nav.rx_feed(data)
        for ptype, _, payload in self.nav.rx_packets():
            if ptype == rn.ACK:
                self.acks.append(self.nav.decode_ack(payload))
            elif ptype == rn.BUTTON:
                print("button", self.nav.decode_button(payload))
            elif ptype == rn.CALL_ACTION:
                self.actions.append(self.nav.decode_call_action(payload))

    def wait_ack(self, ptype, timeout_s):
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            self.poll()
            for i, ack in enumerate(self.acks):
                if ack["type"] == ptype:
                    del self.acks[: i + 1]
                    return ack
        return None


def upload_map(link, pack_len):
    nav = link.nav
    link.send(nav.frame_map_begin)
    ack = link.wait_ack(rn.MAP_BEGIN, 10)
    if not ack or ack["status"] != rn.ACK_OK:
        raise RuntimeError(f"ROTOS refused the map: {ack}")
    chunk, acked, sent = nav.map_chunk_size(), 0, 0
    last_progress = time.time()
    while acked < pack_len:
        while sent < pack_len and sent < acked + WINDOW_CHUNKS * chunk:
            link.send(lambda seq, off=sent: nav.frame_map_chunk(seq, off))
            sent = min(sent + chunk, pack_len)
        ack = link.wait_ack(rn.MAP_CHUNK, 0.2)
        if ack:
            acked, last_progress = ack["value"], time.time()
            sent = max(sent, acked) if ack["status"] == rn.ACK_OK else acked
        elif time.time() - last_progress > 3:
            sent, last_progress = acked, time.time()          # resend the window
    link.send(nav.frame_map_end)
    ack = link.wait_ack(rn.MAP_END, 5)
    if not ack or ack["status"] != rn.ACK_OK:
        raise RuntimeError(f"map transfer failed: {ack}")
    print(f"map uploaded: {pack_len} bytes, CRC confirmed by ROTOS")


class CallScript:
    """Plays timeline.CALLS live: rings until the rider acts on ROTOS or the
    ring time runs out, answers CALL_ACTION like the Android app would."""

    def __init__(self, link, clock):
        self.link, self.clock = link, clock
        self.pending = list(timeline.CALLS)
        self.ringing = None          # (call, started_ms)
        self.next_ring_ms = 0

    def _state(self, t, call, state, flags):
        self.link.send(lambda seq: self.link.nav.frame_call_state(seq, t, call["id"], state, flags, call["name"]))

    def tick(self, t):
        if self.ringing is None and self.pending and t >= self.clock.t0 + self.pending[0]["at_s"] * 1000:
            self.ringing = (self.pending.pop(0), t)
            self.next_ring_ms = t
            print(f"incoming call from {self.ringing[0]['name']}")
        if self.ringing is None:
            return
        call, started = self.ringing
        for cid, action in self.link.actions:
            if cid == call["id"] and action == rn.ACTION_REJECT_REPLY:
                print(f"ROTOS: reject + reply -> hang up, SMS to {call['name']}: "
                      f"'Riding right now, will call you back soon.'")
                self._state(t, call, rn.CALL_IDLE, rn.CALL_F_REPLIED)
                self.ringing = None
            elif cid == call["id"]:
                print("ROTOS: popup dismissed, phone keeps ringing")
        self.link.actions.clear()
        if self.ringing is None:
            return
        if t - started > max(call["ring_s"], 20) * 1000:
            print(f"call from {call['name']} missed")
            self._state(t, call, rn.CALL_IDLE, rn.CALL_F_MISSED)
            self.ringing = None
        elif t >= self.next_ring_ms:
            self._state(t, call, rn.CALL_RINGING, 0)
            self.next_ring_ms = t + timeline.CALL_REPEAT_MS


def run(nav, host, port, pack_len, fixes, clock, wx, speed=1.0, fc=None):
    link = Link(nav, host, port)
    print(f"connected to ROTOS at {host}:{port}")
    t0 = time.time()

    def link_ms():
        return int((time.time() - t0) * 1000 * speed)

    def status():
        now = clock.local(link_ms())
        secs = now.hour * 3600 + now.minute * 60 + now.second
        link.send(lambda seq, t=link_ms(): nav.frame_phone_status(seq, t, secs, now.weekday(), now.day,
                                                                   now.month, 77, 0, 4))
    status()
    link.send(lambda seq: nav.frame_weather(seq, 0, wx["temp_c"], wx["code"], wx["rain_prob"],
                                            wx["wind_kmh"], wx["is_day"]))
    if fc:
        link.send(lambda seq: nav.frame_forecast(seq, 0, fc["start_hour"], fc["sunrise_min"],
                                                 fc["sunset_min"], fc["hours"]))
    upload_map(link, pack_len)
    clock.t0 = link_ms() + 2000
    calls = CallScript(link, clock)
    nav.engine_reset()
    next_status = link_ms()
    for (ts, lat, lon, spd, hdg, acc) in fixes:
        t = clock.t0 + int(ts * 1000)
        while link_ms() < t:
            link.poll()
            calls.tick(link_ms())
            time.sleep(0.01)
        link.send(lambda seq: nav.engine_fix(seq, t, clock.unix(t), clock.off, lat, lon, spd, hdg, acc))
        if link_ms() >= next_status:
            status()
            next_status += 5000
    print("ride finished; the link stays open 20 s for the summary card")
    end = time.time() + 20 / speed
    while time.time() < end:
        link.poll()
        calls.tick(link_ms())
        if link_ms() >= next_status:
            status()
            next_status += 5000
