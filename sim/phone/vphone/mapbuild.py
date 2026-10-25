"""Map pack builder: the recipe from docs/PROTOCOL.md ("Phone side map pack
recipe"), identical to the Android app's MapPackBuilder.

Corridor cells (32 px at zoom 16) are processed in blocks of 8 x 8 cells.
Each block is generated from a 576 x 576 zoom-17 window (the block plus a
16 px halo at zoom 16), so the C filters see real neighbours at block edges.
"""
import numpy as np

ZOOM = 16
BUFFER_PX = 260
CELL = 32
BLOCK_CELLS = 8
BLOCK = CELL * BLOCK_CELLS          # 256 px at zoom 16
HALO = 16
WIN = 2 * (BLOCK + 2 * HALO)        # 576 px at zoom 17


def _kept_blocks(nav, info):
    bw = (info["tiles_w"] + BLOCK_CELLS - 1) // BLOCK_CELLS
    bh = (info["tiles_h"] + BLOCK_CELLS - 1) // BLOCK_CELLS
    blocks = {}
    for cy in range(info["tiles_h"]):
        for cx in range(info["tiles_w"]):
            if nav.corridor_keep(cx, cy):
                blocks.setdefault((cx // BLOCK_CELLS, cy // BLOCK_CELLS), []).append((cx, cy))
    return blocks, bw, bh


def _wanted_tiles(info, cells):
    """zoom-17 tiles touching the kept cells (+ halo) of one block."""
    want = set()
    for cx, cy in cells:
        x0 = (info["origin_x"] + cx * CELL - HALO) * 2
        y0 = (info["origin_y"] + cy * CELL - HALO) * 2
        x1, y1 = x0 + 2 * (CELL + 2 * HALO) - 1, y0 + 2 * (CELL + 2 * HALO) - 1
        for ty in range(y0 // 256, y1 // 256 + 1):
            for tx in range(x0 // 256, x1 // 256 + 1):
                want.add((tx, ty))
    return want


def build(nav, tiles, progress=None):
    """Builds the pack inside the navigation library; returns (pack bytes,
    class grid as a numpy array for previews, corridor info)."""
    n_keep = nav.corridor_plan(ZOOM, BUFFER_PX)
    info = nav.corridor_info()
    blocks, _, _ = _kept_blocks(nav, info)
    grid = np.full((info["tiles_h"] * CELL, info["tiles_w"] * CELL), 255, np.uint8)
    nav.pack_begin()
    for i, ((bx, by), cells) in enumerate(sorted(blocks.items())):
        want = _wanted_tiles(info, cells)
        wx = (info["origin_x"] + bx * BLOCK - HALO) * 2
        wy = (info["origin_y"] + by * BLOCK - HALO) * 2
        rgb = tiles.window(ZOOM + 1, wx, wy, WIN, WIN, lambda tx, ty, w=want: (tx, ty) in w)
        classes = np.frombuffer(nav.mapgen_block(rgb.tobytes(), WIN, WIN), np.uint8).reshape(WIN // 2, WIN // 2)
        for cx, cy in cells:
            lx, ly = (cx - bx * BLOCK_CELLS) * CELL + HALO, (cy - by * BLOCK_CELLS) * CELL + HALO
            tile = np.ascontiguousarray(classes[ly:ly + CELL, lx:lx + CELL])
            nav.pack_add_tile(cx, cy, tile.tobytes())
            grid[cy * CELL:(cy + 1) * CELL, cx * CELL:(cx + 1) * CELL] = tile
        if progress:
            progress(i + 1, len(blocks))
    nav.pack_finish()
    info["wire"] = nav.compress_wire()
    info["n_keep"] = n_keep
    info["blocks"] = len(blocks)
    return nav.pack_bytes(), grid, info
