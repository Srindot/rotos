/*
 * rt_link.c - device end of the phone link.
 */
#include "rt_link.h"

#include <string.h>

#include "hal_link.h"
#include "rn/rn_types.h"
#include "rt_config.h"

void rt_link_init(RtLink *l)
{
    if (!RN_ASSERT(l != NULL)) {
        return;
    }
    (void)memset(l, 0, sizeof *l);
    rn_parser_init(&l->parser);
}

void rt_link_begin_poll(RtLink *l)
{
    if (!RN_ASSERT(l != NULL)) {
        return;
    }
    l->polled = 0u;
}

static bool refill(RtLink *l)
{
    l->rx_pos = 0u;
    l->rx_len = hal_link_read(l->rx, RT_LINK_RX_CHUNK);
    l->rx_len = (l->rx_len > RT_LINK_RX_CHUNK) ? RT_LINK_RX_CHUNK : l->rx_len;
    l->rx_bytes += l->rx_len;
    return l->rx_len > 0u;
}

bool rt_link_next(RtLink *l, uint32_t now_ms, RnPacket *out)
{
    if (!RN_ASSERT(l != NULL && out != NULL) || l->polled >= RT_LINK_MAX_PER_POLL) {
        return false;
    }
    if (rn_parser_drain(&l->parser, out) == RN_PARSE_PACKET) {
        l->polled++;
        l->last_rx_ms = (now_ms != 0u) ? now_ms : 1u;
        return true;
    }
    for (uint32_t guard = 0u; guard < 2u * RT_LINK_RX_CHUNK + 2u; guard++) {
        if (l->rx_pos >= l->rx_len && !refill(l)) {
            return false;
        }
        uint8_t b = l->rx[l->rx_pos];
        l->rx_pos++;
        if (rn_parser_push(&l->parser, b, out) == RN_PARSE_PACKET) {
            l->polled++;
            l->last_rx_ms = (now_ms != 0u) ? now_ms : 1u;
            return true;
        }
    }
    return false;
}

bool rt_link_ok(const RtLink *l, uint32_t now_ms)
{
    if (!RN_ASSERT(l != NULL)) {
        return false;
    }
    return l->last_rx_ms != 0u && now_ms - l->last_rx_ms < RT_LINK_TIMEOUT_MS;
}

static bool send(RtLink *l, uint32_t now_ms, uint8_t type, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[RN_PKT_MAX_FRAME];
    uint32_t n = 0u;
    if (rn_frame_encode(type, l->tx_seq, payload, len, frame, sizeof frame, &n) != RN_OK) {
        return false;
    }
    l->tx_seq++;
    l->last_tx_ms = now_ms;
    l->tx_frames++;
    return hal_link_write(frame, n);
}

bool rt_link_send_ack(RtLink *l, uint32_t now_ms, uint8_t acked_type, uint16_t acked_seq,
                      uint8_t status, uint32_t value)
{
    if (!RN_ASSERT(l != NULL)) {
        return false;
    }
    RnAck a = {acked_type, status, acked_seq, value};
    uint8_t payload[16];
    uint16_t len = 0u;
    if (rn_ack_encode(&a, payload, sizeof payload, &len) != RN_OK) {
        return false;
    }
    return send(l, now_ms, RN_PKT_ACK, payload, len);
}

bool rt_link_send_button(RtLink *l, uint32_t now_ms, uint8_t button, uint8_t event)
{
    if (!RN_ASSERT(l != NULL) || !RN_ASSERT(event == RN_BUTTON_SHORT || event == RN_BUTTON_LONG)) {
        return false;
    }
    RnButton b = {button, event};
    uint8_t payload[4];
    uint16_t len = 0u;
    if (rn_button_encode(&b, payload, sizeof payload, &len) != RN_OK) {
        return false;
    }
    return send(l, now_ms, RN_PKT_BUTTON, payload, len);
}

bool rt_link_send_call_action(RtLink *l, uint32_t now_ms, uint16_t call_id, uint8_t action)
{
    if (!RN_ASSERT(l != NULL) || !RN_ASSERT(action == RN_ACTION_REJECT_REPLY || action == RN_ACTION_DISMISS)) {
        return false;
    }
    RnCallAction a = {call_id, action};
    uint8_t payload[8];
    uint16_t len = 0u;
    if (rn_call_action_encode(&a, payload, sizeof payload, &len) != RN_OK) {
        return false;
    }
    return send(l, now_ms, RN_PKT_CALL_ACTION, payload, len);
}

bool rt_link_send_device_status(RtLink *l, uint32_t now_ms, const RnDeviceStatus *st)
{
    if (!RN_ASSERT(l != NULL && st != NULL)) {
        return false;
    }
    uint8_t payload[16];
    uint16_t len = 0u;
    if (rn_device_status_encode(st, payload, sizeof payload, &len) != RN_OK) {
        return false;
    }
    return send(l, now_ms, RN_PKT_DEVICE_STATUS, payload, len);
}

void rt_link_keepalive(RtLink *l, uint32_t now_ms)
{
    if (!RN_ASSERT(l != NULL)) {
        return;
    }
    if (hal_link_connected() && now_ms - l->last_tx_ms >= RT_PING_MS) {
        (void)send(l, now_ms, RN_PKT_PING, NULL, 0u);
    }
}
