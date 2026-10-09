// rotos_screen — digital twin of a 2.8" ILI9341 240x320 TFT.
//
// Completely independent of ROTOS: it emulates the controller's command set
// (CASET / PASET / RAMWR / SLPOUT / DISPON ...) and GRAM, and shows GRAM
// pixel-exact. Whatever drives a real ILI9341 over SPI can drive this over TCP.
//
//   rotos_screen [--port 9341] [--scale 3] [--shot-on-exit file.bmp]
//   keys: G pixel grid, S screenshot (BMP), ESC/Q quit
#include <SDL.h>
#include <arpa/inet.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <algorithm>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include "spi_wire.h"

constexpr int W = 240, H = 320;

// ── ILI9341 controller model ──────────────────────────────────────────
struct Ili9341Model {
    uint16_t gram[W * H] = {};
    bool sleeping = true, displayOn = false;
    uint8_t cmd = 0;
    uint8_t params[4];
    int nParams = 0;
    int xs = 0, xe = W - 1, ys = 0, ye = H - 1, cx = 0, cy = 0;
    int hiByte = -1;
    uint64_t bytesIn = 0;

    void reset() {
        std::memset(gram, 0, sizeof gram);
        sleeping = true; displayOn = false;
        xs = 0; xe = W - 1; ys = 0; ye = H - 1;
    }
    void command(uint8_t c) {
        cmd = c; nParams = 0; hiByte = -1;
        switch (c) {
        case 0x01: reset(); break;                       // SWRESET
        case 0x10: sleeping = true; break;               // SLPIN
        case 0x11: sleeping = false; break;              // SLPOUT
        case 0x28: displayOn = false; break;             // DISPOFF
        case 0x29: displayOn = true; break;              // DISPON
        case 0x2C: cx = xs; cy = ys; break;              // RAMWR
        default: break;                                  // others: accepted, ignored
        }
    }
    void data(const uint8_t* d, size_t n) {
        bytesIn += n;
        if (cmd == 0x2C) {
            for (size_t i = 0; i < n; ++i) {
                if (hiByte < 0) { hiByte = d[i]; continue; }
                uint16_t px = uint16_t((hiByte << 8) | d[i]);   // big-endian on the wire
                hiByte = -1;
                if (cx < W && cy < H) gram[cy * W + cx] = px;
                if (++cx > xe) { cx = xs; if (++cy > ye) cy = ys; }
            }
            return;
        }
        for (size_t i = 0; i < n && nParams < 4; ++i) params[nParams++] = d[i];
        if (nParams == 4 && (cmd == 0x2A || cmd == 0x2B)) {
            int a = (params[0] << 8) | params[1], b = (params[2] << 8) | params[3];
            if (cmd == 0x2A) { xs = a; xe = b < W ? b : W - 1; }
            else             { ys = a; ye = b < H ? b : H - 1; }
        }
    }
};

// ── frame decoder for the TCP link ────────────────────────────────────
struct Link {
    int listenFd = -1, fd = -1;
    uint8_t hdr[5];
    int hdrHave = 0;
    uint32_t remaining = 0;
    uint8_t kind = 0;

    bool listen(uint16_t port) {
        listenFd = socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listenFd, (sockaddr*)&a, sizeof a) < 0 || ::listen(listenFd, 1) < 0) return false;
        fcntl(listenFd, F_SETFL, O_NONBLOCK);
        return true;
    }
    void poll(Ili9341Model& lcd) {
        if (fd < 0) {
            fd = accept(listenFd, nullptr, nullptr);
            if (fd >= 0) { fcntl(fd, F_SETFL, O_NONBLOCK); hdrHave = 0; remaining = 0; }
            return;
        }
        static uint8_t buf[1 << 16];
        for (;;) {
            ssize_t n = recv(fd, buf, sizeof buf, 0);
            if (n == 0) { close(fd); fd = -1; return; }   // driver went away; GRAM keeps its image
            if (n < 0) return;
            for (ssize_t i = 0; i < n;) {
                if (remaining == 0) {
                    hdr[hdrHave++] = buf[i++];
                    if (hdrHave == 5) {
                        kind = hdr[0];
                        remaining = hdr[1] | (hdr[2] << 8) | (hdr[3] << 16) | (uint32_t(hdr[4]) << 24);
                        hdrHave = 0;
                    }
                    continue;
                }
                size_t take = std::min<size_t>(remaining, size_t(n - i));
                if (kind == screen_wire::FRAME_CMD) lcd.command(buf[i]);
                else lcd.data(buf + i, take);
                i += take;
                remaining -= uint32_t(take);
            }
        }
    }
};

static volatile sig_atomic_t g_quit = 0;
static void onSignal(int) { g_quit = 1; }

static void saveShot(SDL_Renderer* ren, int w, int h, const char* path) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    std::printf("screenshot: %s\n", path);
}

int main(int argc, char** argv) {
    int scale = 3, port = screen_wire::DEFAULT_PORT;
    const char* shotOnExit = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--scale") && i + 1 < argc) scale = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--port") && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--shot-on-exit") && i + 1 < argc) shotOnExit = argv[++i];
    }
    if (scale < 1) scale = 1;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    static Ili9341Model lcd;
    Link link;
    if (!link.listen(uint16_t(port))) { std::perror("listen"); return 1; }
    std::printf("rotos_screen: ILI9341 twin listening on 127.0.0.1:%d\n", port);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Window* win = SDL_CreateWindow("ROTOS screen", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       W * scale, H * scale, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, W, H);
    static uint16_t black[W * H] = {};

    bool grid = false;
    int shots = 0;
    uint32_t statT = SDL_GetTicks();
    uint64_t statBytes = 0;
    while (!g_quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g_quit = 1;
            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE || k == SDLK_q) g_quit = 1;
                if (k == SDLK_g) grid = !grid;
                if (k == SDLK_s) {
                    char p[64];
                    std::snprintf(p, sizeof p, "rotos_screen_%03d.bmp", shots++);
                    saveShot(ren, W * scale, H * scale, p);
                }
            }
        }
        link.poll(lcd);

        uint32_t now = SDL_GetTicks();
        if (now - statT >= 1000) {
            double mbps = (lcd.bytesIn - statBytes) / 1e6 * 1000.0 / (now - statT);
            char title[160];
            // 40 MHz SPI moves at most 5 MB/s; show how much of it ROTOS uses.
            std::snprintf(title, sizeof title, "ROTOS screen  |  ILI9341 240x320  |  %s  |  SPI %.2f MB/s (%.0f%% of 40 MHz)",
                          link.fd >= 0 ? "linked" : "no link", mbps, mbps / 5.0 * 100);
            SDL_SetWindowTitle(win, title);
            statT = now;
            statBytes = lcd.bytesIn;
        }

        bool lit = lcd.displayOn && !lcd.sleeping;
        SDL_UpdateTexture(tex, nullptr, lit ? lcd.gram : black, W * 2);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        if (grid && scale >= 3) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 70);
            for (int x = 0; x <= W; ++x) SDL_RenderDrawLine(ren, x * scale, 0, x * scale, H * scale);
            for (int y = 0; y <= H; ++y) SDL_RenderDrawLine(ren, 0, y * scale, W * scale, y * scale);
        }
        SDL_RenderPresent(ren);
    }
    if (shotOnExit) saveShot(ren, W * scale, H * scale, shotOnExit);
    SDL_Quit();
    return 0;
}
