"""Capture files (.rfeed): timestamped wire bytes, like a tiny pcap.

    record = u32 deliver_ms | u16 len | bytes[len]

Delivery adds link latency and loses a few packets, so the device's
dead reckoning and loss accounting get exercised. Map transfer frames are
never lost or reordered (BLE retransmits at the link layer; a lost chunk
would be resent by the app's ACK window anyway).
"""
import struct

import rotos_nav as rn

RELIABLE = (rn.MAP_BEGIN, rn.MAP_CHUNK, rn.MAP_END)


def deliver(frames, rnd, loss=0.03):
    """[(t, type, bytes)] -> [(deliver_ms, bytes)] sorted by delivery."""
    out = []
    for i, (t, ptype, data) in enumerate(frames):
        if ptype in RELIABLE:
            out.append((t + 30, i, data))
            continue
        if i > 0 and rnd.random() < loss:
            continue
        out.append((t + 80 + rnd.randint(0, 120), i, data))
    out.sort(key=lambda r: (r[0], r[1]))
    return [(d, data) for d, _, data in out]


def encode(records):
    return b"".join(struct.pack("<IH", d, len(data)) + data for d, data in records)
