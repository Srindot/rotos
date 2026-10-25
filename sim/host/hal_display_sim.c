/*
 * hal_display_sim.c - virtual SPI: every command / data transfer becomes a
 * sim_wire frame to the screen twin over TCP. Headless mode only counts
 * bytes. The twin's back channel (buttons, light sensor) is read here too.
 */
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "../screen/sim_wire.h"
#include "hal_display.h"
#include "sim_config.h"

#define OUT_CAP (256u * 1024u)

static int s_fd = -1;
static uint8_t s_out[OUT_CAP];
static uint32_t s_out_len;
static uint64_t s_bytes;
static uint8_t s_buttons;
static int32_t s_light = -1;
static uint8_t s_rx_hdr[SIM_WIRE_HEADER + 1u];
static uint32_t s_rx_have;

static bool try_connect(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }
    struct sockaddr_in a;
    (void)memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&a, sizeof a) != 0) {
        (void)close(fd);
        return false;
    }
    int one = 1;
    (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    s_fd = fd;
    return true;
}

bool hal_display_init(void)
{
    const SimConfig *c = sim_config();
    if (c->headless) {
        return true;
    }
    for (int i = 0; i < 25; i++) {                       /* 5 s for the twin to start */
        if (try_connect(c->screen_port)) {
            return true;
        }
        struct timespec ts = {0, 200000000L};
        (void)nanosleep(&ts, NULL);
    }
    return false;
}

static void push_frame(uint8_t kind, const uint8_t *bytes, uint32_t n)
{
    s_bytes += n;
    if (s_fd < 0) {
        return;
    }
    if (s_out_len + SIM_WIRE_HEADER + n > OUT_CAP) {
        hal_display_flush();
    }
    uint8_t h[SIM_WIRE_HEADER] = {kind, (uint8_t)n, (uint8_t)(n >> 8), (uint8_t)(n >> 16), (uint8_t)(n >> 24)};
    if (SIM_WIRE_HEADER + n > OUT_CAP) {
        return;
    }
    (void)memcpy(&s_out[s_out_len], h, SIM_WIRE_HEADER);
    (void)memcpy(&s_out[s_out_len + SIM_WIRE_HEADER], bytes, n);
    s_out_len += SIM_WIRE_HEADER + n;
}

void hal_display_reset(void)
{
    /* the twin has no RST pin; SWRESET in the init sequence resets it */
}

void hal_display_command(uint8_t cmd)
{
    push_frame(SIM_WIRE_CMD, &cmd, 1u);
}

void hal_display_data(const uint8_t *bytes, uint32_t n)
{
    if (bytes != NULL && n > 0u) {
        push_frame(SIM_WIRE_DATA, bytes, n);
    }
}

void hal_display_flush(void)
{
    uint32_t off = 0u;
    for (int k = 0; k < 100000 && s_fd >= 0 && off < s_out_len; k++) {
        ssize_t n = send(s_fd, &s_out[off], s_out_len - off, MSG_NOSIGNAL);
        if (n <= 0) {
            (void)close(s_fd);
            s_fd = -1;
            break;
        }
        off += (uint32_t)n;
    }
    s_out_len = 0u;
}

uint8_t hal_display_madctl(void)
{
    return 0x48u;                          /* the twin ignores orientation */
}

void hal_display_backlight(uint8_t pct)
{
    push_frame(SIM_WIRE_BACKLIGHT, &pct, 1u);
    hal_display_flush();
}

void sim_display_poll(void)
{
    for (int k = 0; k < 64 && s_fd >= 0; k++) {
        uint8_t b;
        ssize_t n = recv(s_fd, &b, 1u, MSG_DONTWAIT);
        if (n <= 0) {
            return;
        }
        s_rx_hdr[s_rx_have] = b;
        s_rx_have++;
        if (s_rx_have == SIM_WIRE_HEADER + 1u) {          /* all back frames carry 1 byte */
            if (s_rx_hdr[0] == SIM_WIRE_BUTTONS) {
                s_buttons = s_rx_hdr[5];
            } else if (s_rx_hdr[0] == SIM_WIRE_LIGHT) {
                s_light = (s_rx_hdr[5] > 100u) ? -1 : (int32_t)s_rx_hdr[5];
            }
            s_rx_have = 0u;
        }
    }
}

uint64_t sim_display_bytes(void)
{
    return s_bytes;
}

bool sim_display_ok(void)
{
    return sim_config()->headless || s_fd >= 0;
}

uint8_t sim_display_buttons(void)
{
    return s_buttons;
}

int32_t sim_display_light(void)
{
    return s_light;
}
