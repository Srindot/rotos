# ROTOS

## Custom Embedded Motorcycle Navigation & Instrument System

ROTOS is a custom-built, motorcycle-mounted navigation and information system designed for the **Triumph Speed 400**.

The project combines an Android smartphone, an ESP32-based embedded controller, a small color TFT display, wireless communication, geographic/map data, and a custom graphics engine to create a dedicated motorcycle navigation instrument.

The fundamental idea is:

> **The phone performs the complex computation. ROTOS performs the embedded visualization.**

Instead of mounting a smartphone on the motorcycle or simply mirroring Google Maps onto a second screen, ROTOS receives structured navigation information from the phone and renders its own map and navigation interface.

The result is intended to feel like a **purpose-built motorcycle instrument**, rather than an Arduino display attached to a motorcycle.

---

# 1. Motivation

Modern smartphone navigation works extremely well, but using a smartphone directly on a motorcycle has several disadvantages:

- The phone has to remain exposed and mounted.
- The display consumes significant space on the handlebar.
- The phone is exposed to vibration, weather, and heat.
- A conventional smartphone interface is not designed specifically for glanceable motorcycle use.
- The rider may need to interact with the phone while riding.
- The navigation interface cannot be customized at the embedded-system level.

ROTOS addresses this by separating the system into two computers:

```text
                  SMARTPHONE
                      │
              High-level intelligence
                      │
          ┌───────────┴───────────┐
          │                       │
       Routing                  GPS
          │                       │
          └───────────┬───────────┘
                      │
                Navigation data
                      │
                    Wi-Fi
                      │
                      ▼
                  ROTOS
                      │
              Embedded graphics
                      │
                      ▼
                    TFT
```

The smartphone can remain inside a pocket or bag while ROTOS provides the rider with the information required for navigation.

---

# 2. Project Objectives

The primary objective is to design and build a compact embedded navigation system capable of displaying a motorcycle's route and navigation information in real time.

The system should:

1. Receive navigation information from a smartphone.
2. Receive and maintain the full route geometry.
3. Track the motorcycle's position.
4. Display the route spatially rather than showing only the next turn.
5. Show upcoming maneuvers.
6. Display useful ride information such as:
   - Speed
   - Distance
   - ETA
   - Road name
   - Navigation status
7. Render maps using a custom graphics engine.
8. Communicate with the phone wirelessly.
9. Operate from the motorcycle's existing USB-C power system.
10. Fit inside a custom 3D-printed enclosure.
11. Be expandable into a broader motorcycle information/telemetry system.

---

# 3. Design Philosophy

ROTOS is deliberately **not** intended to be a miniature smartphone.

The project should take advantage of the constraints of the embedded display rather than fighting them.

The display is approximately:

```text
240 × 320 pixels
```

Therefore, instead of trying to reproduce Google Maps pixel-for-pixel, ROTOS will develop its own visual language.

Possible visual styles include:

- Minimal
- Pixel-art
- Retro
- Cyberpunk
- Racing
- Dark navigation
- Japanese-inspired
- Custom themes

The goal is to create something that is:

> **Useful + technically interesting + visually distinctive.**

---

# 4. Name

## ROTOS

ROTOS is inspired by the electronic/robotic aesthetic of **Pokémon's Rotom**, while remaining an independent project name.

The name is intentionally short so that it works well as:

- A project name
- A boot screen
- An enclosure logo
- A firmware name
- A future hardware/software platform name

Example boot screen:

```text
┌──────────────────────────────┐
│                              │
│            ROTOS             │
│                              │
│      MOTORCYCLE SYSTEM       │
│                              │
│         INITIALIZING         │
│                              │
└──────────────────────────────┘
```

---

# 5. High-Level Architecture

The system consists of two major computing platforms.

## 5.1 Smartphone

The smartphone acts as the **high-level navigation computer**.

It is responsible for:

- GPS
- Route calculation
- Navigation provider integration
- Route geometry
- Maneuvers
- ETA
- Remaining distance
- Rerouting
- Map data processing
- Communication with ROTOS

## 5.2 ROTOS Embedded System

ROTOS acts as the **real-time embedded display computer**.

It is responsible for:

- Wireless communication
- Navigation state management
- Route caching
- Map rendering
- UI rendering
- TFT control
- Display refresh
- Connection management

---

# 6. Overall System

```text
                         SMARTPHONE
                    ┌─────────────────────┐
                    │                     │
                    │       GPS           │
                    │                     │
                    │ Navigation Provider │
                    │                     │
                    │ Google Routes API   │
                    │        /            │
                    │     OSM Data        │
                    │                     │
                    └──────────┬──────────┘
                               │
                               ▼
                    ┌─────────────────────┐
                    │ Navigation Adapter  │
                    │                     │
                    │ Route               │
                    │ Maneuvers           │
                    │ Position            │
                    │ Heading             │
                    │ ETA                 │
                    │ Distance            │
                    └──────────┬──────────┘
                               │
                               ▼
                    ┌─────────────────────┐
                    │ Navigation Model    │
                    │                     │
                    │ Route Geometry      │
                    │ Maneuver List       │
                    │ Current Position    │
                    │ Current Segment     │
                    └──────────┬──────────┘
                               │
                            Wi-Fi
                               │
                               ▼
                    ┌─────────────────────┐
                    │       ROTOS         │
                    │                     │
                    │ Communication       │
                    │ Protocol            │
                    │                     │
                    │ Navigation State    │
                    │ Route Manager       │
                    │                     │
                    │ Map Renderer        │
                    │ UI Renderer         │
                    └──────────┬──────────┘
                               │
                              SPI
                               │
                               ▼
                    ┌─────────────────────┐
                    │      TFT LCD        │
                    │                     │
                    │    240 × 320        │
                    │      ILI9341        │
                    └─────────────────────┘
```

---

# 7. Hardware

## 7.1 Microcontroller

### Initial Controller

**Seeed Studio XIAO ESP32-C3**

The XIAO ESP32-C3 provides:

- Wi-Fi
- Bluetooth Low Energy
- USB
- SPI
- GPIO
- 3.3 V operation
- Small physical footprint
- Low power consumption

The C3 is sufficient for the initial prototype.

### Possible Final Controller

**XIAO ESP32-S3 with PSRAM**

The ESP32-S3 is a potential upgrade if the map renderer becomes computationally demanding.

The software architecture should therefore avoid unnecessary dependence on the exact MCU.

---

# 8. Display

The initial display target is:

**2.8-inch ILI9341 TFT**

Target specifications:

```text
Resolution:      240 × 320
Interface:       SPI
Color format:    RGB565
Logic voltage:   3.3 V
Display type:    TFT LCD
```

The display will be responsible for rendering:

- Roads
- Route
- Current position
- Maneuver arrows
- Road names
- Speed
- ETA
- Distance
- Status indicators
- Custom UI elements

---

# 9. Motorcycle Power

ROTOS will use the existing USB-C power output of the Triumph Speed 400.

```text
Triumph Speed 400
       │
       │ USB-C
       ▼
    ROTOS MCU
       │
       │ 3.3 V
       ▼
      TFT
```

The motorcycle's USB-C power system is expected to be used only while the motorcycle is operating.

Therefore ROTOS does not require a separate battery for normal operation.

The final installation should still verify the actual behavior of the motorcycle's USB-C socket and its power limits.

The embedded system should also handle sudden power loss safely.

---

# 10. Bill of Materials

| Component | Specification | Purpose |
|---|---|---|
| Microcontroller | Seeed XIAO ESP32-C3 | Main embedded controller |
| Optional MCU upgrade | XIAO ESP32-S3 + PSRAM | Higher-performance final platform |
| Display | 2.8" ILI9341 TFT | Navigation display |
| USB cable | USB-C to USB-C | Power/programming |
| Enclosure | 3D printed ABS | Protection |
| Motorcycle mount | Custom 3D printed | Physical mounting |
| Wiring | Silicone/JST wires | Internal connections |
| Fasteners | M3 hardware | Mechanical assembly |
| Vibration isolation | Rubber/silicone mounts | Reduce vibration |
| Optional sensor | IMU | Future telemetry |
| Optional sensor | Ambient light sensor | Automatic brightness |

---

# 11. Phone Software

The smartphone application is the high-level software component.

The application should be structured around abstraction rather than directly coupling the entire system to a single navigation provider.

```text
NavigationProvider
       │
       ├── GoogleProvider
       │
       ├── OSMProvider
       │
       └── MockProvider
```

This allows development to begin using simulated navigation data before integrating a real navigation provider.

---

# 12. Navigation Data Model

The system should represent the **entire route**, not just the next maneuver.

A route can be represented as:

```text
P0 → P1 → P2 → P3 → P4 → ... → Pn
```

where each `Pi` is a geographic coordinate.

The route also contains maneuver information.

Example:

```text
Route
│
├── Geometry
│     ├── P0
│     ├── P1
│     ├── P2
│     └── ...
│
├── Maneuvers
│     ├── LEFT  @ 180 m
│     ├── RIGHT @ 600 m
│     └── LEFT  @ 1.2 km
│
├── Total distance
├── Remaining distance
└── Current segment
```

---

# 13. Navigation State

A simplified navigation state can contain:

```cpp
struct NavigationState
{
    bool valid;

    double latitude;
    double longitude;

    float heading;
    float speed;

    float distance_to_maneuver;

    float remaining_distance;

    uint32_t eta;

    char road_name[64];

    bool rerouting;
};
```

---

# 14. Route Representation

A route contains geometry and maneuvers.

```cpp
struct Route
{
    Polyline geometry;

    std::vector<Maneuver> maneuvers;

    float total_distance;
    float remaining_distance;

    int current_segment;
};
```

A maneuver can be represented as:

```cpp
struct Maneuver
{
    Coordinate position;

    ManeuverType type;

    float distance_from_current;
};
```

---

# 15. Map Architecture

The project separates three different concepts:

## 15.1 Map Data

Map data describes the physical world.

Possible source:

**OpenStreetMap**

This provides information such as:

- Roads
- Intersections
- Road geometry
- Geographic features

## 15.2 Routing

Routing determines which roads the motorcycle should follow.

Possible source:

**Google Routes API**

The route response can provide:

- Route geometry
- Polyline
- Maneuvers
- Distance
- ETA

## 15.3 Rendering

ROTOS renders the information using its own graphics engine.

Therefore:

```text
            Google
              │
              │ Route
              ▼
        Route Polyline
              │
              ▼
        Navigation Model
              │
              │
              ├──────────────┐
              │              │
              ▼              ▼
        Route Geometry     OSM Map
              │              │
              └──────┬───────┘
                     │
                     ▼
                Map Renderer
                     │
                     ▼
                    TFT
```

---

# 16. Why the Full Route Is Required

A simple navigation system could display only:

```text
TURN LEFT
250 m
```

However, this does not provide enough spatial context.

ROTOS should instead display the route ahead:

```text
                     ───────
                           ╲
                            ╲
                  ──────────╲
                             ╲
                              ╲
                               ▲
                              YOU
```

This allows the rider to visually understand:

- The direction of the route
- Upcoming turns
- Road curvature
- Relative route geometry
- Multiple upcoming maneuvers

The route therefore becomes a first-class data structure in ROTOS.

---

# 17. Google Routes API

The Google routing system can be used to obtain route geometry rather than attempting to extract the pixels of the Google Maps application.

The system should therefore avoid:

```text
Google Maps screen
       ↓
Screen capture
       ↓
Video/image streaming
       ↓
TFT
```

Instead:

```text
Google Routes API
       ↓
Route geometry
       ↓
Navigation model
       ↓
Custom renderer
       ↓
TFT
```

This is significantly more efficient and gives ROTOS control over its visual appearance.

---

# 18. Route Request Strategy

Google route requests should not be made continuously.

The intended strategy is:

```text
Destination selected
        ↓
Route request
        ↓
Full route received
        ↓
Route cached
        ↓
GPS updated locally
        ↓
Navigation state updated
        ↓
ESP32 updated
        ↓
Route request only when:
    • destination changes
    • rerouting is required
    • route becomes invalid
```

This avoids unnecessary API usage.

The monthly route request allowance is expected to be far above the requirements of a personal navigation device.

---

# 19. Communication

Wi-Fi will be the primary communication mechanism between the smartphone and ROTOS.

BLE may be implemented later for low-bandwidth control or fallback communication.

The communication protocol should be independent of the navigation provider.

---

# 20. Communication Packet

A compact packet format can be used:

```text
┌──────┬─────────┬──────┬──────────┬─────────┬─────┐
│ SYNC │ VERSION │ TYPE │ SEQUENCE │ PAYLOAD │ CRC │
└──────┴─────────┴──────┴──────────┴─────────┴─────┘
```

Where:

- `SYNC` identifies the beginning of a packet.
- `VERSION` identifies the protocol version.
- `TYPE` identifies the message.
- `SEQUENCE` detects dropped/out-of-order packets.
- `PAYLOAD` contains the data.
- `CRC` provides error detection.

Possible packet types:

```text
NAVIGATION_STATE
ROUTE_UPDATE
PHONE_STATUS
MEDIA_STATE
SETTINGS
PING
ACK
```

---

# 21. Route Transmission

The complete route does not need to be transmitted continuously.

Instead:

```text
PHONE
  │
  │ ROUTE_UPDATE
  ▼
ROTOS
  │
  ▼
Route cached locally
```

After that:

```text
PHONE
  │
  │ Current position / navigation state
  ▼
ROTOS
```

Only a new route needs a new `ROUTE_UPDATE`.

This reduces wireless traffic and allows the embedded system to continue rendering the route even during short communication interruptions.

---

# 22. ESP32 Firmware

The embedded firmware should be modular.

```text
ROTOS Firmware
│
├── Communication
│
├── Protocol Parser
│
├── Navigation State Manager
│
├── Route Manager
│
├── Map Renderer
│
├── UI Manager
│
└── Hardware Drivers
       │
       └── ILI9341
```

The communication layer should not directly manipulate the display.

Instead:

```text
Communication
      ↓
Navigation State
      ↓
Renderer
      ↓
TFT
```

This separation makes the system easier to debug and extend.

---

# 23. Display State Machine

The display should have explicit operating states.

```text
             ┌─────────────┐
             │   STARTUP   │
             └──────┬──────┘
                    ▼
             ┌─────────────┐
             │ DISCONNECTED│
             └──────┬──────┘
                    │
                    │ Phone connected
                    ▼
             ┌─────────────┐
             │  CONNECTED  │
             └──────┬──────┘
                    │
                    │ Route available
                    ▼
             ┌─────────────┐
             │ NAVIGATION  │
             └──────┬──────┘
                    │
          ┌─────────┴─────────┐
          ▼                   ▼
     REROUTING             ARRIVING
```

Possible additional states:

```text
ERROR
UPDATING
NO GPS
LOW CONNECTION
DESTINATION REACHED
```

---

# 24. Graphics Engine

The graphics engine should provide a small abstraction layer over the TFT.

Basic primitives:

```cpp
drawPixel();
drawLine();
drawPolyline();
drawRect();
drawCircle();
drawText();
drawBitmap();
```

Higher-level rendering components can then be built on top:

```text
MapRenderer
UIRenderer
IconRenderer
TextRenderer
```

---

# 25. Map Rendering Pipeline

The map renderer converts geographic coordinates into screen coordinates.

```text
Latitude / Longitude
        ↓
Coordinate Transformation
        ↓
Local Map Coordinates
        ↓
Viewport Transformation
        ↓
Clipping
        ↓
Screen Coordinates
        ↓
TFT Pixels
```

The renderer must eventually support:

1. Geographic-to-local transformation
2. Map projection
3. Viewport management
4. Polyline clipping
5. Road rendering
6. Route highlighting
7. Current-position marker
8. Maneuver icons
9. Labels
10. Zoom
11. Rotation

---

# 26. Initial User Interface

The first navigation UI should prioritize clarity.

Example:

```text
┌──────────────────────────────┐
│  ← LEFT             240 m    │
│                              │
│          ╲                   │
│           ╲                  │
│    ────────╲────────         │
│             ╲                │
│              ╲──────         │
│                    ●         │
│                              │
│       MG ROAD                │
│                              │
│  42 km/h       ETA 18:42     │
│  12.4 km        4.2 km       │
└──────────────────────────────┘
```

The UI should remain readable at a glance.

---

# 27. Visual Themes

The rendering architecture should allow themes to be changed without changing the underlying navigation system.

Potential themes:

```text
MINIMAL
PIXEL
RETRO
CYBERPUNK
RACING
DARK
JAPANESE
```

For example, the same navigation state can be rendered as:

```text
Navigation State
       │
       ├── Minimal Renderer
       │
       ├── Pixel Renderer
       │
       ├── Racing Renderer
       │
       └── Cyberpunk Renderer
```

---

# 28. Phone Connection

The phone should remain physically hidden while providing:

```text
GPS
Navigation
Internet connectivity
Route calculation
Map data
```

ROTOS only needs to receive the information necessary to display the current navigation state.

The phone therefore acts as the system's:

> **High-level computer**

while ROTOS acts as the:

> **Real-time embedded instrument**

---

# 29. Prototype Development Strategy

The system should be developed incrementally.

Do not attempt to build the entire navigation stack at once.

The recommended progression is:

```text
TFT
 ↓
ESP32 graphics
 ↓
Mock navigation
 ↓
Map renderer
 ↓
Wi-Fi communication
 ↓
Android application
 ↓
GPS
 ↓
Google route integration
 ↓
OSM map integration
 ↓
Motorcycle integration
```

This allows each layer to be tested independently.

---

# 30. Implementation Phases

## Phase 1 — TFT Bring-Up

Tasks:

- [ ] Connect XIAO ESP32-C3
- [ ] Connect ILI9341
- [ ] Implement SPI driver
- [ ] Initialize display
- [ ] Render pixels
- [ ] Render lines
- [ ] Render rectangles
- [ ] Render text
- [ ] Measure refresh performance

Expected result:

```text
ESP32-C3 → SPI → ILI9341

Hello ROTOS
Speed: 42 km/h
```

---

## Phase 2 — Graphics Engine

Implement:

```text
drawPixel()
drawLine()
drawPolyline()
drawRect()
drawCircle()
drawText()
drawBitmap()
```

Then create:

```text
MapRenderer
UIRenderer
IconRenderer
TextRenderer
```

---

## Phase 3 — Mock Navigation

Create fake routes.

Example:

```text
START
  ↓
STRAIGHT
  ↓
LEFT
  ↓
STRAIGHT
  ↓
RIGHT
  ↓
DESTINATION
```

Implement the navigation data model before integrating real navigation.

---

## Phase 4 — Wi-Fi

Implement:

```text
Phone
  │
  │ Wi-Fi
  ▼
ROTOS
```

Start with:

```text
PING
```

Then:

```text
NAVIGATION_STATE
```

Then:

```text
ROUTE_UPDATE
```

Test:

- Latency
- Packet loss
- Reconnection
- Sequence numbers
- CRC
- Connection timeout

---

## Phase 5 — Android Companion

Implement:

```text
Android App
│
├── GPS Manager
├── Navigation Provider
├── Route Manager
├── Navigation State
└── Communication Manager
```

Initially use mock navigation data.

---

## Phase 6 — Google Integration

Integrate the Google routing backend.

The phone should obtain:

```text
Route
 ├── Full route polyline
 ├── Maneuvers
 ├── Distance
 ├── ETA
 └── Route metadata
```

Convert this information into the ROTOS navigation model.

The embedded system should not know whether the information originated from Google or a mock provider.

---

## Phase 7 — OSM Integration

Introduce surrounding road geometry.

The phone can process the map data and transmit only the relevant local map region to ROTOS.

This prevents the ESP32 from having to store an entire city map.

---

## Phase 8 — Custom Map Renderer

Implement:

- Coordinate transformation
- Map projection
- Viewport
- Zoom
- Rotation
- Road rendering
- Route rendering
- Route highlighting
- Current-position marker
- Maneuver markers
- Labels
- Clipping

---

## Phase 9 — Physical Integration

Once the electronics and software work on a desk:

- [ ] Design enclosure
- [ ] 3D print ABS enclosure
- [ ] Design handlebar mount
- [ ] Add vibration isolation
- [ ] Integrate USB-C power
- [ ] Route internal wiring
- [ ] Verify display visibility
- [ ] Verify thermal performance
- [ ] Test Wi-Fi reliability
- [ ] Test power behavior

---

# 31. Mechanical Design

The final ROTOS enclosure should be designed specifically for the Speed 400.

The enclosure should provide:

- TFT protection
- ESP32 protection
- Cable management
- Mounting points
- Ventilation where necessary
- Vibration isolation
- Weather resistance
- Easy serviceability

The enclosure can initially be 3D printed using ABS.

Future versions may use:

- ASA
- PETG
- CNC-machined parts
- Custom injection-molded housing

---

# 32. Motorcycle Integration

The final system should be mounted so that it is:

- Within the rider's normal field of view
- Secure under vibration
- Protected from accidental impact
- Not obstructing motorcycle controls
- Not obstructing the instrument cluster
- Not interfering with steering
- Easy to remove

The display should be designed for **glanceable information**, not interaction while riding.

---

# 33. Future Extensions

The architecture intentionally allows ROTOS to grow beyond navigation.

## Vehicle Telemetry

Potential future information:

```text
Speed
RPM
Gear
Engine temperature
Fuel
Trip distance
Average speed
Ride time
```

---

## Sensors

Potential additions:

```text
IMU
External GPS
Ambient light sensor
Temperature sensor
```

---

## Phone Integration

Potential features:

```text
Phone battery
Incoming calls
Music
Media controls
Notifications
```

---

## Advanced Navigation

Potential features:

```text
Lane visualization
Turn animations
Automatic zoom
Automatic rotation
3D perspective
Traffic information
Alternative routes
Waypoint navigation
```

---

## Advanced Hardware

Potential future revisions:

```text
Custom PCB
Integrated power management
Dedicated buttons
Rotary encoder
Ambient light sensor
Higher-resolution display
Sunlight-readable display
ESP32-S3 + PSRAM
```

---

# 34. Final Architecture

The intended final architecture is:

```text
                         SMARTPHONE
                              │
                    ┌─────────┴─────────┐
                    │                   │
                   GPS              Navigation
                    │                   │
                    │            Google / OSM
                    │                   │
                    └─────────┬─────────┘
                              │
                              ▼
                     Navigation Model
                              │
                     Full Route Geometry
                              │
                     Maneuvers / ETA
                              │
                              ▼
                        Wi-Fi Protocol
                              │
                              ▼
                    ┌───────────────────┐
                    │       ROTOS       │
                    │                   │
                    │ Communication     │
                    │ State Manager     │
                    │ Route Manager     │
                    │ Map Renderer      │
                    │ UI Renderer       │
                    └─────────┬─────────┘
                              │
                             SPI
                              │
                              ▼
                    ┌───────────────────┐
                    │    2.8" ILI9341   │
                    │     240 × 320     │
                    └───────────────────┘
```

---

# 35. Core Design Principle

ROTOS follows a strict separation between **navigation intelligence** and **embedded visualization**.

```text
PHONE
=
Navigation + GPS + Internet + Route computation

ROTOS
=
Communication + State + Rendering + UI

TFT
=
Human interface
```

This separation allows the system to remain lightweight, modular, and expandable.

---

# 36. Final Objective

The final product should be a compact, custom-built motorcycle instrument that provides the rider with a clear view of their route without requiring the smartphone to be mounted on the motorcycle.

The project combines:

- Embedded systems
- ESP32 development
- Wireless communication
- Android development
- GPS
- Geographic data
- Route geometry
- Map rendering
- Computer graphics
- 3D mechanical design
- Motorcycle integration

The intended result is not simply a navigation screen.

It is:

> **ROTOS — a custom embedded navigation and information platform built specifically for the motorcycle.**
