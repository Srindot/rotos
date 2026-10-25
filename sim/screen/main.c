/*
 * rotos_screen - digital twin of a 2.8" ILI9341 240x320 TFT with three
 * buttons and an ambient light sensor.
 *
 * Completely independent of ROTOS: it emulates the controller's command set
 * and GRAM and shows GRAM pixel-exact. Whatever drives a real ILI9341 over
 * SPI can drive this over TCP (see sim_wire.h).
 *
 *   rotos_screen [--port 9341] [--scale 3] [--shot-on-exit file.bmp]
 *   keys: 1 2 3 buttons A B C (hold for a long press)
 *         L ambient light: sun / shade / dark / no sensor
 *         G pixel grid, S screenshot (BMP), ESC/Q quit
 *
 * main() runs the modules in order: options -> LCD model -> wire server ->
 * SDL window -> loop (events, wire, draw).
 */
#include <SDL.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ili9341_model.h"
#include "sim_wire.h"
#include "wire_server.h"

typedef struct {
    int scale;
    int port;
    const char *shot_on_exit;
} Options;

typedef struct {
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *tex;
    bool grid;
    int shots;
    uint8_t buttons;
    int light_step;
} View;

static const uint8_t LIGHT_LEVELS[4] = {100u, 45u, 8u, 255u};
static const char *const LIGHT_NAMES[4] = {"sun", "shade", "dark", "no sensor"};
static volatile sig_atomic_t g_quit;

static void on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

static Options parse(int argc, char **argv)
{
    Options o = {3, SIM_WIRE_PORT, NULL};
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "--scale") == 0) {
            o.scale = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0) {
            o.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--shot-on-exit") == 0) {
            o.shot_on_exit = argv[++i];
        }
    }
    o.scale = (o.scale < 1) ? 1 : o.scale;
    return o;
}

static void save_shot(View *v, int w, int h, const char *path)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (s == NULL) {
        return;
    }
    (void)SDL_RenderReadPixels(v->ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    (void)SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    (void)printf("screenshot: %s\n", path);
}

static uint8_t key_button(SDL_Keycode k)
{
    return (k == SDLK_1) ? 1u : ((k == SDLK_2) ? 2u : ((k == SDLK_3) ? 4u : 0u));
}

static void handle_events(View *v, WireServer *w, const Options *o)
{
    SDL_Event e;
    for (int k = 0; k < 256 && SDL_PollEvent(&e) != 0; k++) {
        if (e.type == SDL_QUIT) {
            g_quit = 1;
        }
        if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.repeat == 0) {
            SDL_Keycode key = e.key.keysym.sym;
            uint8_t b = key_button(key);
            if (b != 0u) {
                v->buttons = (e.type == SDL_KEYDOWN) ? (uint8_t)(v->buttons | b) : (uint8_t)(v->buttons & ~b);
                wire_send(w, SIM_WIRE_BUTTONS, v->buttons);
            }
            if (e.type != SDL_KEYDOWN) {
                continue;
            }
            if (key == SDLK_ESCAPE || key == SDLK_q) {
                g_quit = 1;
            } else if (key == SDLK_g) {
                v->grid = !v->grid;
            } else if (key == SDLK_l) {
                v->light_step = (v->light_step + 1) % 4;
                wire_send(w, SIM_WIRE_LIGHT, LIGHT_LEVELS[v->light_step]);
                (void)printf("ambient light: %s\n", LIGHT_NAMES[v->light_step]);
            } else if (key == SDLK_s) {
                char p[64];
                (void)snprintf(p, sizeof p, "rotos_screen_%03d.bmp", v->shots++);
                save_shot(v, LCD_W * o->scale, LCD_H * o->scale, p);
            }
        }
    }
}

/* GRAM through the backlight: dimmed panel = darker pixels. */
static void draw(View *v, const LcdModel *m, const Options *o)
{
    static uint16_t frame[LCD_W * LCD_H];
    bool lit = lcd_lit(m);
    uint32_t bl = (m->backlight == 0u) ? 100u : m->backlight;
    for (int i = 0; i < LCD_W * LCD_H && lit; i++) {
        uint16_t c = m->gram[i];
        uint32_t r = ((c >> 11) & 31u) * bl / 100u;
        uint32_t g = ((c >> 5) & 63u) * bl / 100u;
        uint32_t b = (c & 31u) * bl / 100u;
        frame[i] = (uint16_t)((r << 11) | (g << 5) | b);
    }
    if (!lit) {
        (void)memset(frame, 0, sizeof frame);
    }
    (void)SDL_UpdateTexture(v->tex, NULL, frame, LCD_W * 2);
    (void)SDL_RenderClear(v->ren);
    (void)SDL_RenderCopy(v->ren, v->tex, NULL, NULL);
    if (v->grid && o->scale >= 3) {
        (void)SDL_SetRenderDrawBlendMode(v->ren, SDL_BLENDMODE_BLEND);
        (void)SDL_SetRenderDrawColor(v->ren, 0, 0, 0, 70);
        for (int x = 0; x <= LCD_W; x++) {
            (void)SDL_RenderDrawLine(v->ren, x * o->scale, 0, x * o->scale, LCD_H * o->scale);
        }
        for (int y = 0; y <= LCD_H; y++) {
            (void)SDL_RenderDrawLine(v->ren, 0, y * o->scale, LCD_W * o->scale, y * o->scale);
        }
    }
    SDL_RenderPresent(v->ren);
}

static void update_title(View *v, const LcdModel *m, const WireServer *w)
{
    static uint32_t t0;
    static uint64_t b0;
    uint32_t now = SDL_GetTicks();
    if (now - t0 < 1000u) {
        return;
    }
    double mbps = (double)(m->bytes_in - b0) / 1e6 * 1000.0 / (double)(now - t0);
    char title[200];
    (void)snprintf(title, sizeof title,
                   "ROTOS screen | ILI9341 240x320 | %s | SPI %.2f MB/s (%.0f%% of 40 MHz) | backlight %u%%",
                   wire_linked(w) ? "linked" : "no link", mbps, mbps / 5.0 * 100.0,
                   (unsigned)((m->backlight == 0u) ? 100u : m->backlight));
    SDL_SetWindowTitle(v->win, title);
    t0 = now;
    b0 = m->bytes_in;
}

static bool open_view(View *v, const Options *o)
{
    (void)memset(v, 0, sizeof *v);
    v->light_step = 3;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        (void)fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    (void)SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    v->win = SDL_CreateWindow("ROTOS screen", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              LCD_W * o->scale, LCD_H * o->scale, 0);
    v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    v->tex = SDL_CreateTexture(v->ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, LCD_W, LCD_H);
    return v->win != NULL && v->ren != NULL && v->tex != NULL;
}

int main(int argc, char **argv)
{
    static LcdModel lcd;
    static WireServer wire;
    View view;
    Options opt = parse(argc, argv);
    (void)signal(SIGINT, on_signal);
    (void)signal(SIGTERM, on_signal);
    lcd_reset(&lcd);
    if (!wire_listen(&wire, (uint16_t)opt.port)) {
        perror("listen");
        return 1;
    }
    (void)printf("rotos_screen: ILI9341 twin on 127.0.0.1:%d  (keys: 1 2 3 buttons, L light, G grid, S shot)\n",
                 opt.port);
    if (!open_view(&view, &opt)) {
        return 1;
    }
    while (g_quit == 0) {                          /* JPL-R2 exempt: main loop */
        handle_events(&view, &wire, &opt);
        bool was_linked = wire_linked(&wire);
        wire_poll(&wire, &lcd);
        if (!was_linked && wire_linked(&wire)) {
            wire_send(&wire, SIM_WIRE_LIGHT, LIGHT_LEVELS[view.light_step]);
        }
        update_title(&view, &lcd, &wire);
        draw(&view, &lcd, &opt);
    }
    if (opt.shot_on_exit != NULL) {
        save_shot(&view, LCD_W * opt.scale, LCD_H * opt.scale, opt.shot_on_exit);
    }
    SDL_Quit();
    return 0;
}
