/*
 * rt_link.h - the device end of the phone link: pulls bytes from hal_link,
 * yields complete packets one at a time, and sends ACK / BUTTON / PING.
 */
#ifndef RT_LINK_H
#define RT_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "rn/rn_protocol.h"

#define RT_LINK_RX_CHUNK     512u
#define RT_LINK_MAX_PER_POLL 64u     /* packets handled per frame, at most */

typedef struct {
    RnParser parser;
    uint8_t rx[RT_LINK_RX_CHUNK];
    uint32_t rx_len;
    uint32_t rx_pos;
    uint16_t tx_seq;
    uint32_t last_rx_ms;       /* 0 = never                      */
    uint32_t last_tx_ms;
    uint32_t rx_bytes;
    uint32_t tx_frames;
    uint32_t polled;           /* packets returned this poll     */
} RtLink;

void rt_link_init(RtLink *l);
/* Start a new poll (once per frame). */
void rt_link_begin_poll(RtLink *l);
/* Next complete packet, false when nothing more is available this frame. */
bool rt_link_next(RtLink *l, uint32_t now_ms, RnPacket *out);
bool rt_link_ok(const RtLink *l, uint32_t now_ms);

bool rt_link_send_ack(RtLink *l, uint32_t now_ms, uint8_t acked_type, uint16_t acked_seq,
                      uint8_t status, uint32_t value);
bool rt_link_send_button(RtLink *l, uint32_t now_ms, uint8_t button, uint8_t event);
bool rt_link_send_call_action(RtLink *l, uint32_t now_ms, uint16_t call_id, uint8_t action);
bool rt_link_send_device_status(RtLink *l, uint32_t now_ms, const RnDeviceStatus *st);
/* PING when nothing was sent for RT_PING_MS. */
void rt_link_keepalive(RtLink *l, uint32_t now_ms);

#endif /* RT_LINK_H */
