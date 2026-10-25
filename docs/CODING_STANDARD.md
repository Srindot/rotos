# ROTOS coding standard

ROTOS follows NASA/JPL's **"Power of Ten" rules for safety-critical code**
(G. Holzmann, JPL, 2006) for all C code in `core/`, `firmware/` and `sim/`.
The Kotlin Android app follows the same spirit, adapted to Kotlin (last section).

The rules exist so that every piece of code can be reasoned about, tested and
checked by tools. A motorcycle display is not a spacecraft, but it runs for
hours on a vibrating bike with nobody to press reset, and it is
the same kind of small embedded system the rules were written for.

`tools/nasa_check.py` checks the mechanical rules automatically and runs
cppcheck; run it before committing (`python3 tools/nasa_check.py`).

## Where ROTOS bends the rules (on purpose)

- **Vendor callbacks (R9):** NimBLE and the ESP-IDF drivers require callback
  function pointers. They appear only in `firmware/hal/esp/`; the checker
  allows them there and nowhere else.
- **Assertion density (R5):** every public function validates its parameters,
  but the average is about 0.7 assertions per function, not 2: tiny static
  helpers rely on their already-validated callers. `nasa_check.py` prints the
  density per directory so it can be raised over time.
- **Main loops (R2):** the frame loops in `firmware/main/main.c`,
  `sim/host/main.c` and the screen twin never end by design; they're marked
  `JPL-R2 exempt`.
- **Host-only code** (`sim/`, Python, Kotlin) follows the style but uses OS
  services (sockets, files, SDL) freely.

## The ten rules, as applied here

| # | JPL rule | How ROTOS applies it |
|---|---|---|
| 1 | Simple control flow: no `goto`, `setjmp`/`longjmp`, no recursion | Banned. Flood fills and searches use explicit, bounded work stacks. |
| 2 | Every loop has a fixed upper bound | Loops run over sized arrays (`for (i = 0; i < n && i < RN_MAX_...; ...)`). The only unbounded loop is the firmware's main scheduler loop, marked `/* JPL-R2 exempt: main loop */`. |
| 3 | No dynamic memory allocation after initialisation | No `malloc`/`free` anywhere, with one boot-time exception: on the classic ESP32 `firmware/main/main.c` takes the 64 KB tile cache from the heap once, before BLE starts, and never frees it (the chip caps static RAM at ~160 KB). All other memory is static or provided by the caller (workspace structs). Sizes are compile-time limits in `rn_types.h` / `rt_config.h`. |
| 4 | Functions fit on one page (≤ 60 lines) | Enforced by `nasa_check.py`. |
| 5 | ≥ 2 assertions per function on average | `RN_ASSERT(cond)` returns `false` (never aborts in flight) and is used to validate parameters and invariants; the caller then returns an error. Trivial accessors may have none; complex functions have more. |
| 6 | Data declared at the smallest possible scope | Module state is `static` in its `.c` file; locals are declared where used. |
| 7 | Check every non-void return value; validate every parameter | Status codes (`rn_status`) are checked or explicitly cast to `(void)` with a reason. |
| 8 | Limited preprocessor: includes, simple constants, the assert macro | No token pasting, no variadic macros. Macro "functions" only for `RN_ASSERT` and constant-expression helpers in static tables (`HEX()` in the palettes). |
| 9 | Restrict pointers: one level of dereference, **no function pointers** | Platform differences are bound at **link time** (one `hal_*.h` header, one implementation per platform), not through callbacks. Parsers return results instead of calling handlers. |
| 10 | Compile with all warnings, zero warnings; static analysis daily | `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` (host), `cppcheck` via `nasa_check.py`. |

## Style that follows from it

- **Functional core, imperative shell.** Algorithms are functions of their
  inputs: `out = f(in)`, no hidden globals, no I/O. Side effects (SPI, BLE,
  flash, sockets, files, HTTP) live only in the HAL / platform layer and in
  `main`. This is why the same navigation code runs in the simulator, on the
  ESP32 and inside the Android app.
- **One responsibility per file.** Each `.c` file is one module with one header.
  Each `main` calls the modules' `init` functions in a fixed, documented order,
  then runs the loop.
- **Naming.** `rn_` navigation library, `rt_` ROTOS firmware application,
  `hal_` hardware abstraction, `sim_` simulator only. Types are `RnThing` /
  `RtThing`, functions `rn_module_verb()`, constants `RN_UPPER_CASE`.
- **Fixed-width types** (`uint8_t`, `int32_t`, ...) for everything that is
  stored or sent; `float` for geometry on the device (the ESP32-C3 has no
  FPU; doubles are used only on the phone side for latitude/longitude).
- **Little-endian wire formats** are read and written byte by byte
  (`rn_wire.h`), never by casting buffers to structs.
- **Errors are values.** Functions that can fail return `rn_status`
  (`RN_OK` = 0, negative = error). Nothing aborts on the device.

## Kotlin (android/)

- Immutable `data class`es and pure functions for logic; Android APIs
  (location, BLE, sockets, HTTP) only at the edges, behind small interfaces.
- One class or concern per file, grouped by task in packages (`ride/`,
  `quota/`, `route/`, `map/`, `mirror/`, `link/`, ...). `RideService` only
  wires them together in order; screens live in `ui/android` and only draw.
- Rules that decide something (reroute, usage caps, route matching, parsing)
  are pure functions over plain data, with JVM unit tests; the caller owns
  the clock and the state between calls.
- No reflection. Process-wide state lives in a few named, thread-safe
  objects only: `GuidanceBus` (Google Maps turns), `UsageGuard` (Google
  usage), `AppLog`, `AppIdentity`.
- Every billed Google call asks `UsageGuard.acquire` first; when it says no,
  the feature degrades (no new map) instead of retrying.
- Every network call has a timeout; every failure becomes a typed result
  (`Result<T>`), never an uncaught exception.
- All navigation maths runs in the shared C library through JNI
  (`NativeNav.kt`), so the phone and the simulator compute the same answers.
