// rotos_sim — runs the ROTOS core on the desktop.
//
// Plays the phone's part from files produced by phone/build.py and drives
// the rotos_screen twin over virtual SPI. On hardware, only this file and
// tcp_spi_bus.cpp change: bytes come from Wi-Fi, pixels go to real SPI.
//
//   rotos_sim [--map out/route.rmap] [--feed out/ride.rfeed]
//             [--theme auto|day|night] [--north-up] [--speed 1.0] [--port 9341]
//             [--glass 70]   panel opacity %, 0 = clear .. 100 = solid
//   rotos_sim --headless --snap 3000,60000 --snap-dir out/snaps   (no screen)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>
#include "rotos.h"
#include "tcp_spi_bus.h"

using namespace rotos;

static std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

struct FeedRecord { uint32_t deliverMs; std::vector<uint8_t> bytes; };

static std::vector<FeedRecord> parseFeed(const std::vector<uint8_t>& raw) {
    std::vector<FeedRecord> out;
    for (size_t i = 0; i + 6 <= raw.size();) {
        uint32_t t;
        uint16_t n;
        std::memcpy(&t, &raw[i], 4);
        std::memcpy(&n, &raw[i + 4], 2);
        i += 6;
        if (i + n > raw.size()) break;
        out.push_back({t, {raw.begin() + long(i), raw.begin() + long(i + n)}});
        i += n;
    }
    return out;
}

static void writePpm(const Canvas& cv, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", Canvas::W, Canvas::H);
    uint16_t row[Canvas::W];
    for (int y = 0; y < Canvas::H; ++y) {
        cv.rowRgb565(y, row);
        for (int x = 0; x < Canvas::W; ++x) {
            uint16_t c = row[x];
            uint8_t r = c >> 11, g = (c >> 5) & 63, b = c & 31;
            uint8_t px[3] = {uint8_t(r << 3 | r >> 2), uint8_t(g << 2 | g >> 4), uint8_t(b << 3 | b >> 2)};
            std::fwrite(px, 1, 3, f);
        }
    }
    std::fclose(f);
}

int main(int argc, char** argv) {
    std::string mapPath = "out/route.rmap", feedPath = "out/ride.rfeed", themeName = "auto";
    std::string snapDir = "out/snaps";
    std::vector<uint32_t> snaps;
    bool northUp = false, headless = false;
    double speed = 1.0;
    int port = 9341, glass = 70;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--map") mapPath = next();
        else if (a == "--feed") feedPath = next();
        else if (a == "--theme") themeName = next();
        else if (a == "--north-up") northUp = true;
        else if (a == "--speed") speed = std::atof(next().c_str());
        else if (a == "--port") port = std::atoi(next().c_str());
        else if (a == "--glass") glass = std::atoi(next().c_str());
        else if (a == "--headless") headless = true;
        else if (a == "--snap-dir") snapDir = next();
        else if (a == "--snap") {
            std::string s = next();
            for (char* p = std::strtok(&s[0], ","); p; p = std::strtok(nullptr, ",")) snaps.push_back(uint32_t(std::atol(p)));
        } else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }

    std::vector<uint8_t> mapBlob = readFile(mapPath);
    static MapPack map;
    if (!map.load(mapBlob.data(), mapBlob.size())) {
        std::fprintf(stderr, "cannot load map pack %s (run: python3 phone/build.py)\n", mapPath.c_str());
        return 1;
    }
    std::vector<FeedRecord> feed = parseFeed(readFile(feedPath));
    std::printf("map: %d x %d tiles, %zu route points, %zu maneuvers, %.1f km | feed: %zu packets\n",
                map.tilesW, map.tilesH, map.route.size(), map.maneuvers.size(), map.totalM / 1000, feed.size());

    TcpSpiBus tcp;
    NullSpiBus null;
    SpiBus* bus = &null;
    if (!headless) {
        if (!tcp.connect("127.0.0.1", port, 5000)) {
            std::fprintf(stderr, "no screen on port %d: start ./build/rotos_screen first\n", port);
            return 1;
        }
        bus = &tcp;
    }
    Ili9341 lcd(*bus);
    static Rotos app(lcd, map);   // ~40 KiB framebuffer: keep it off the stack
    app.setThemeMode(themeName == "day" ? Rotos::ThemeMode::Day
                     : themeName == "night" ? Rotos::ThemeMode::Night : Rotos::ThemeMode::Auto);
    app.setNorthUp(northUp);
    app.setGlassOpacity(glass);
    app.begin();

    // The phone link comes up a little after boot finishes.
    const uint32_t FEED_START_MS = 2600, FRAME_MS = 33;
    size_t fi = 0;
    uint32_t frames = 0, arrivedAt = 0;
    uint64_t spiBytes0 = 0;
    auto t0 = std::chrono::steady_clock::now();
    if (!snaps.empty()) std::filesystem::create_directories(snapDir);
    size_t si = 0;

    for (uint32_t simMs = 0;; ) {
        if (headless) simMs += FRAME_MS;
        else simMs = uint32_t(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() * speed);

        while (fi < feed.size() && feed[fi].deliverMs + FEED_START_MS <= simMs) {
            app.receive(feed[fi].bytes.data(), feed[fi].bytes.size(), simMs);
            ++fi;
        }
        bool wasNight = app.isNight();
        app.tick(simMs);
        if (app.isNight() != wasNight)
            std::printf("theme -> %s at %.1f s sim time\n", app.isNight() ? "night" : "day", simMs / 1000.0);
        ++frames;
        if (frames == 60) spiBytes0 = null.bytes;  // skip boot for the stats

        while (si < snaps.size() && simMs >= snaps[si]) {
            char p[512];
            std::snprintf(p, sizeof p, "%s/%s_%06u.ppm", snapDir.c_str(), themeName.c_str(), snaps[si]);
            writePpm(app.canvas(), p);
            std::printf("snap %s\n", p);
            ++si;
        }
        if (app.state() == Rotos::State::Summary && !arrivedAt) {
            arrivedAt = simMs;
            std::printf("arrived at %.1f min sim time | link: %u ok, %u lost, %u crc errors\n",
                        simMs / 60000.0, app.link().ok, app.link().dropped, app.link().crcErrors);
        }
        if (headless) {
            bool snapsDone = si >= snaps.size();
            if (snaps.empty() ? (arrivedAt && simMs > arrivedAt + 3000) : (snapsDone || (arrivedAt && simMs > arrivedAt + 120000))) break;
            continue;
        }
        if (!tcp.ok()) { std::printf("screen closed\n"); break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_MS));
    }

    if (headless && frames > 60) {
        double perFrame = double(null.bytes - spiBytes0) / (frames - 60);
        std::printf("SPI: %.1f KiB/frame avg -> %.1f ms/frame at 40 MHz (full frame = 150 KiB, 30.7 ms)\n",
                    perFrame / 1024, perFrame * 8 / 40e6 * 1000);
    }
    return 0;
}
