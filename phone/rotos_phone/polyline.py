"""Google encoded polyline format (precision 1e5)."""


def decode(s):
    pts, i, lat, lng = [], 0, 0, 0
    while i < len(s):
        for axis in (0, 1):
            shift = result = 0
            while True:
                b = ord(s[i]) - 63
                i += 1
                result |= (b & 0x1F) << shift
                shift += 5
                if b < 0x20:
                    break
            d = ~(result >> 1) if result & 1 else result >> 1
            if axis == 0:
                lat += d
            else:
                lng += d
        pts.append((lat / 1e5, lng / 1e5))
    return pts
