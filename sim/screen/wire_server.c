/*
 * wire_server.c - TCP side of the screen twin.
 */
#include "wire_server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sim_wire.h"

#define RX_BUF (1 << 16)

bool wire_listen(WireServer *w, uint16_t port)
{
    (void)memset(w, 0, sizeof *w);
    w->fd = -1;
    w->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (w->listen_fd < 0) {
        return false;
    }
    int one = 1;
    (void)setsockopt(w->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a;
    (void)memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(w->listen_fd, (struct sockaddr *)&a, sizeof a) < 0 || listen(w->listen_fd, 1) < 0) {
        return false;
    }
    return fcntl(w->listen_fd, F_SETFL, O_NONBLOCK) == 0;
}

static void consume(WireServer *w, LcdModel *m, const uint8_t *buf, uint32_t n)
{
    uint32_t i = 0u;
    while (i < n) {
        if (w->remaining == 0u) {
            w->hdr[w->hdr_have] = buf[i];
            w->hdr_have++;
            i++;
            if (w->hdr_have == SIM_WIRE_HEADER) {
                w->kind = w->hdr[0];
                w->remaining = (uint32_t)w->hdr[1] | ((uint32_t)w->hdr[2] << 8) |
                               ((uint32_t)w->hdr[3] << 16) | ((uint32_t)w->hdr[4] << 24);
                w->hdr_have = 0u;
            }
            continue;
        }
        uint32_t take = (w->remaining < n - i) ? w->remaining : n - i;
        if (w->kind == SIM_WIRE_CMD) {
            lcd_command(m, buf[i]);
        } else if (w->kind == SIM_WIRE_DATA) {
            lcd_data(m, &buf[i], take);
        } else if (w->kind == SIM_WIRE_BACKLIGHT) {
            m->backlight = buf[i + take - 1u];
        } else {
            /* unknown frame kinds are skipped */
        }
        i += take;
        w->remaining -= take;
    }
}

void wire_poll(WireServer *w, LcdModel *m)
{
    static uint8_t buf[RX_BUF];
    if (w->fd < 0) {
        w->fd = accept(w->listen_fd, NULL, NULL);
        if (w->fd >= 0) {
            (void)fcntl(w->fd, F_SETFL, O_NONBLOCK);
            w->hdr_have = 0u;
            w->remaining = 0u;
        }
        return;
    }
    for (int k = 0; k < 64; k++) {
        ssize_t n = recv(w->fd, buf, sizeof buf, 0);
        if (n == 0) {
            (void)close(w->fd);                    /* driver went away; GRAM keeps its image */
            w->fd = -1;
            return;
        }
        if (n < 0) {
            return;
        }
        consume(w, m, buf, (uint32_t)n);
    }
}

void wire_send(WireServer *w, uint8_t kind, uint8_t value)
{
    if (w->fd < 0) {
        return;
    }
    uint8_t f[6] = {kind, 1u, 0u, 0u, 0u, value};
    (void)send(w->fd, f, sizeof f, MSG_NOSIGNAL);
}

bool wire_linked(const WireServer *w)
{
    return w->fd >= 0;
}
