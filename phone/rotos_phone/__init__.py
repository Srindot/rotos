"""ROTOS phone-side pipeline (Python prototype of the future Android app).

Google route  ─┐
               ├─► fuse + quantize ─► map pack (.rmap) ─► ROTOS
OSM raster    ─┘
GPS (simulated) ─► localization ─► NAVIGATION_STATE packets (.rfeed) ─► ROTOS
"""
