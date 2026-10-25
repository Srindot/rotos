/*
 * wire_server.h - accepts one driver connection and decodes sim_wire frames
 * into the LCD model; sends button / light frames back.
 */
#ifndef WIRE_SERVER_H
#define WIRE_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#include "ili9341_model.h"

typedef struct {
    int listen_fd;
    int fd;
    uint8_t hdr[5];
    uint32_t hdr_have;
    uint32_t remaining;
    uint8_t kind;
} WireServer;

bool wire_listen(WireServer *w, uint16_t port);
/* Non-blocking: accept / read everything available into the model. */
void wire_poll(WireServer *w, LcdModel *m);
void wire_send(WireServer *w, uint8_t kind, uint8_t value);
bool wire_linked(const WireServer *w);

#endif /* WIRE_SERVER_H */
