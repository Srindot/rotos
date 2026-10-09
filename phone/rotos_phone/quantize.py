"""Turn an OSM raster into a clean semantic pixel map.

OSM tiles are styled for a big screen: anti-aliasing, labels, POI icons,
dashed footpaths. ROTOS wants the opposite: a handful of flat classes that a
theme can recolour, Game Boy style. Steps:

  1. classify every colour to a semantic class (or UNKNOWN for text/icons)
  2. 2x majority downsample (z17 -> z16): thin text strokes vanish
  3. inpaint UNKNOWN from neighbours, remove speckles
  4. drop "roads" 2 px wide or less: footpaths and label halos, not rideable
  5. draw 1 px outlines around roads (the Pokemon look)
"""
import numpy as np
from scipy import ndimage

# Semantic map classes — palette indices 0..7, must match core/theme.h
LAND, LANDUSE, PARK, WATER, BUILDING, ROAD_EDGE, ROAD_MINOR, ROAD_MAJOR = range(8)
UNKNOWN = 255
NAMES = ["land", "landuse", "park", "water", "building", "road_edge", "road_minor", "road_major"]

# openstreetmap-carto colours -> class
REFERENCE = [
    ((242, 239, 233), LAND),       # background
    ((238, 240, 213), LAND),       # farmland
    ((224, 223, 223), LANDUSE),    # residential
    ((242, 218, 217), LANDUSE),    # commercial
    ((255, 214, 209), LANDUSE),    # retail
    ((235, 219, 232), LANDUSE),    # industrial / railway
    ((199, 199, 180), LANDUSE),    # construction / brownfield
    ((223, 221, 206), LANDUSE),    # garages
    ((238, 238, 238), LANDUSE),    # parking
    ((221, 221, 232), LANDUSE),    # pedestrian area
    ((255, 255, 229), LANDUSE),    # education / hospital grounds
    ((246, 238, 183), LANDUSE),    # school/university outline fill variants
    ((200, 250, 204), PARK),       # park
    ((205, 235, 176), PARK),       # grass / meadow
    ((173, 209, 158), PARK),       # forest
    ((200, 215, 171), PARK),       # scrub
    ((170, 224, 203), PARK),       # pitch
    ((222, 246, 192), PARK),       # golf / village green
    ((170, 203, 175), PARK),       # cemetery
    ((223, 252, 226), PARK),       # recreation ground
    ((170, 211, 223), WATER),
    ((217, 208, 201), BUILDING),
    ((196, 182, 171), BUILDING),   # building outline
    ((255, 255, 255), ROAD_MINOR), # tertiary / residential / service
    ((247, 250, 191), ROAD_MAJOR), # secondary
    ((252, 214, 164), ROAD_MAJOR), # primary
    ((249, 178, 156), ROAD_MAJOR), # trunk
    ((232, 146, 162), ROAD_MAJOR), # motorway
]
MAX_DIST = 22.0
# On a tie the earlier class in this list wins: keeps 1 px roads connected.
PRIORITY = [ROAD_MAJOR, ROAD_MINOR, WATER, BUILDING, PARK, LANDUSE, LAND, ROAD_EDGE]


def classify(rgb):
    ref = np.array([c for c, _ in REFERENCE], np.float32)
    cls = np.array([k for _, k in REFERENCE], np.uint8)
    flat = rgb.reshape(-1, 3)
    uniq, inv = np.unique(flat, axis=0, return_inverse=True)
    d = np.linalg.norm(uniq[:, None, :].astype(np.float32) - ref[None], axis=2)
    best = d.argmin(1)
    lut = np.where(d[np.arange(len(uniq)), best] <= MAX_DIST, cls[best], UNKNOWN).astype(np.uint8)
    return lut[inv.ravel()].reshape(rgb.shape[:2])


def _neighbour_counts(a, classes, diag=True):
    """counts[c][y, x] = number of 8- (or 4-) neighbours of (y, x) with class c."""
    p = np.pad(a, 1, constant_values=UNKNOWN)
    h, w = a.shape
    offs = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]
    if not diag:
        offs = [(-1, 0), (0, -1), (0, 1), (1, 0)]
    out = {}
    for c in classes:
        acc = np.zeros(a.shape, np.uint8)
        for dy, dx in offs:
            acc += p[1 + dy:1 + dy + h, 1 + dx:1 + dx + w] == c
        out[c] = acc
    return out


def _vote(counts):
    """Argmax over classes with PRIORITY tie-break; -1 where all counts are 0."""
    score = None
    for rank, c in enumerate(PRIORITY):
        if c not in counts:
            continue
        s = counts[c].astype(np.int32) * 16 + (15 - rank)
        s[counts[c] == 0] = -1
        if score is None:
            score, best = s, np.full(s.shape, c, np.int32)
        else:
            m = s > score
            score[m], best[m] = s[m], c
    best[score < 0] = -1
    return best


def downsample2(a):
    h, w = a.shape[0] // 2 * 2, a.shape[1] // 2 * 2
    b = a[:h, :w].reshape(h // 2, 2, w // 2, 2)
    counts = {c: (b == c).sum((1, 3)).astype(np.uint8) for c in range(8)}
    best = _vote(counts)
    return np.where(best < 0, UNKNOWN, best).astype(np.uint8)


def inpaint(a, iterations=30, classes=range(8)):
    a = a.copy()
    for _ in range(iterations):
        unk = a == UNKNOWN
        if not unk.any():
            break
        best = _vote(_neighbour_counts(a, classes))
        fill = unk & (best >= 0)
        a[fill] = best[fill]
    a[a == UNKNOWN] = LAND
    return a


def despeckle(a, passes=2):
    for _ in range(passes):
        counts = _neighbour_counts(a, range(8))
        same = np.zeros(a.shape, np.uint8)
        for c in range(8):
            same[a == c] = counts[c][a == c]
        lonely = same == 0
        best = _vote(counts)
        a = np.where(lonely & (best >= 0), best, a).astype(np.uint8)
    return a


def thin_roads_out(a, min_blob=60):
    """Remove road pixels that are not part of a real road network:
    - morphological opening drops anything under 3 px wide (footways, halos)
    - small disconnected blobs go too (leftover footpath dashes, icons)
    The removed pixels take the area class around them."""
    road = (a == ROAD_MINOR) | (a == ROAD_MAJOR)
    keep = ndimage.binary_opening(road, np.ones((3, 3), bool))
    lab, n = ndimage.label(keep, np.ones((3, 3), bool))
    sizes = np.bincount(lab.ravel(), minlength=n + 1)
    keep &= sizes[lab] >= min_blob
    a = a.copy()
    a[road & ~keep] = UNKNOWN
    return inpaint(a, classes=(LAND, LANDUSE, PARK, WATER, BUILDING))


def harmonize_roads(a, passes=2):
    """Label halos inside a major road classify as minor: absorb them."""
    for _ in range(passes):
        n = _neighbour_counts(a, (ROAD_MAJOR,))[ROAD_MAJOR]
        a = np.where((a == ROAD_MINOR) & (n >= 3), ROAD_MAJOR, a).astype(np.uint8)
    return a


def outline_roads(a):
    road = (a == ROAD_MINOR) | (a == ROAD_MAJOR)
    p = np.pad(road, 1)
    h, w = a.shape
    near = p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]
    a = a.copy()
    a[near & ~road] = ROAD_EDGE
    return a


def semantic_map(rgb_hi):
    """rgb at zoom z+1 -> class map at zoom z."""
    c = classify(rgb_hi)
    c = downsample2(c)
    c = inpaint(c)
    c = harmonize_roads(c)
    c = thin_roads_out(c)
    c = despeckle(c)
    return outline_roads(c)
