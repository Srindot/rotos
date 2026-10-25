/*
 * hal_link_sim.c - the phone link in the simulator:
 *   FEED: replays a .rfeed capture (records: u32 deliver_ms, u16 len, bytes)
 *         against the device clock, starting at feed_start_ms;
 *   TCP : a server on listen_port for one live phone (the Android app in an
 *         emulator, or the virtual phone in --live mode). Bytes both ways.
 */
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "hal_link.h"
#include "hal_log.h"
#include "hal_time.h"
#include "sim_config.h"

#define FEED_CAP (32u * 1024u * 1024u)

static uint8_t s_feed[FEED_CAP];
static uint32_t s_feed_len;
static uint32_t s_pos;            /* start of the current record          */
static uint32_t s_in_rec;         /* bytes of it already delivered        */
static uint32_t s_end_ms;
static int s_listen = -1;
static int s_fd = -1;

static bool load_feed(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    s_feed_len = (uint32_t)fread(s_feed, 1u, FEED_CAP, f);
    (void)fclose(f);
    for (uint32_t p = 0u; p + 6u <= s_feed_len;) {     /* find the last timestamp */
        uint32_t t = (uint32_t)s_feed[p] | ((uint32_t)s_feed[p + 1u] << 8) |
                     ((uint32_t)s_feed[p + 2u] << 16) | ((uint32_t)s_feed[p + 3u] << 24);
        uint32_t n = (uint32_t)s_feed[p + 4u] | ((uint32_t)s_feed[p + 5u] << 8);
        s_end_ms = t;
        p += 6u + n;
    }
    hal_log("link", "feed loaded, bytes", (int32_t)s_feed_len);
    return true;
}

static bool open_server(uint16_t port)
{
    s_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen < 0) {
        return false;
    }
    int one = 1;
    (void)setsockopt(s_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a;
    (void)memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s_listen, (struct sockaddr *)&a, sizeof a) != 0 || listen(s_listen, 1) != 0) {
        return false;
    }
    hal_log("link", "listening for the phone on TCP port", (int32_t)port);
    return fcntl(s_listen, F_SETFL, O_NONBLOCK) == 0;
}

bool hal_link_init(void)
{
    const SimConfig *c = sim_config();
    if (c->link_mode == SIM_LINK_FEED) {
        return c->feed_path != NULL && load_feed(c->feed_path);
    }
    if (c->link_mode == SIM_LINK_TCP) {
        return open_server(c->listen_port);
    }
    return true;
}

static uint32_t read_feed(uint8_t *buf, uint32_t cap)
{
    uint32_t now = hal_time_ms();
    uint32_t start = sim_config()->feed_start_ms;
    uint32_t n = 0u;
    while (n < cap && s_pos + 6u <= s_feed_len) {
        uint32_t t = (uint32_t)s_feed[s_pos] | ((uint32_t)s_feed[s_pos + 1u] << 8) |
                     ((uint32_t)s_feed[s_pos + 2u] << 16) | ((uint32_t)s_feed[s_pos + 3u] << 24);
        uint32_t len = (uint32_t)s_feed[s_pos + 4u] | ((uint32_t)s_feed[s_pos + 5u] << 8);
        if (t + start > now || s_pos + 6u + len > s_feed_len) {
            break;
        }
        uint32_t take = (len - s_in_rec < cap - n) ? len - s_in_rec : cap - n;
        (void)memcpy(&buf[n], &s_feed[s_pos + 6u + s_in_rec], take);
        n += take;
        s_in_rec += take;
        if (s_in_rec == len) {
            s_pos += 6u + len;
            s_in_rec = 0u;
        }
    }
    return n;
}

static uint32_t read_tcp(uint8_t *buf, uint32_t cap)
{
    if (s_fd < 0) {
        s_fd = accept(s_listen, NULL, NULL);
        if (s_fd >= 0) {
            int one = 1;
            (void)setsockopt(s_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
            (void)fcntl(s_fd, F_SETFL, O_NONBLOCK);
            hal_log("link", "phone connected", 0);
        }
        return 0u;
    }
    ssize_t n = recv(s_fd, buf, cap, 0);
    if (n == 0) {
        (void)close(s_fd);
        s_fd = -1;
        hal_log("link", "phone disconnected", 0);
        return 0u;
    }
    return (n > 0) ? (uint32_t)n : 0u;
}

uint32_t hal_link_read(uint8_t *buf, uint32_t cap)
{
    if (buf == NULL || cap == 0u) {
        return 0u;
    }
    const SimConfig *c = sim_config();
    if (c->link_mode == SIM_LINK_FEED) {
        return read_feed(buf, cap);
    }
    return (c->link_mode == SIM_LINK_TCP) ? read_tcp(buf, cap) : 0u;
}

bool hal_link_write(const uint8_t *bytes, uint32_t n)
{
    if (s_fd < 0 || bytes == NULL) {
        return false;                       /* replay: the capture cannot hear us */
    }
    return send(s_fd, bytes, n, MSG_NOSIGNAL) == (ssize_t)n;
}

bool hal_link_connected(void)
{
    return s_fd >= 0;
}

bool sim_link_feed_done(void)
{
    return sim_config()->link_mode == SIM_LINK_FEED && s_pos + 6u > s_feed_len;
}

uint32_t sim_link_feed_end_ms(void)
{
    return s_end_ms + sim_config()->feed_start_ms;
}
