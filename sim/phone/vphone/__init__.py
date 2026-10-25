"""Virtual phone for the ROTOS simulator.

Plays the Android app's part on the desktop: route in, map pack built,
simulated GPS ride, every packet produced by the shared C navigation
library (core/python/rotos_nav.py). Modules do one job each; the
orchestration lives in sim/phone/rotos_phone.py.
"""
