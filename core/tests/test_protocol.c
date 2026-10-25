/*
 * test_protocol.c - framing, codecs, parser resync and loss accounting.
 */
#include <string.h>

#include "rn/rn_crc.h"
#include "rn/rn_line.h"
#include "rn/rn_maneuver.h"
#include "rn/rn_protocol.h"
#include "rn_test.h"

static uint32_t make_nav_frame(uint16_t seq, float along, uint8_t *out)
{
    RnNavState s;
    memset(&s, 0, sizeof s);
    s.t_ms = 1234u;
    s.route_id = 0xCAFEu;
    s.lat = 17.4455123;
    s.lon = 78.3489456;
    s.speed_mps = 11.5f;
    s.heading_deg = 359.99f;
    s.along_m = along;
    s.remaining_m = 1000.0f - along;
    s.remaining_s = 90u;
    s.eta_s = 86400u + 61u;
    s.flags = RN_NAV_GPS_OK | RN_NAV_NIGHT;
    s.accuracy_m = 5u;
    uint8_t payload[RN_PKT_MAX_PAYLOAD];
    uint16_t len = 0u;
    CHECK(rn_nav_state_encode(&s, payload, sizeof payload, &len) == RN_OK);
    CHECK(len == 38u);
    uint32_t n = 0u;
    CHECK(rn_frame_encode(RN_PKT_NAV_STATE, seq, payload, len, out, RN_PKT_MAX_FRAME, &n) == RN_OK);
    return n;
}

static void test_crc(void)
{
    const uint8_t msg[] = "123456789";
    CHECK(rn_crc16(msg, 9u, RN_CRC16_INIT) == 0x29B1u);       /* CCITT-FALSE check value */
    CHECK(rn_crc32(msg, 9u) == 0xCBF43926u);                 /* IEEE check value */
}

static void test_roundtrip_with_garbage(void)
{
    uint8_t stream[1024];
    uint32_t n = 0u;
    const uint8_t junk[] = {0x00, 0xA5, 0x13, 0xA5, 0xA5, 0x5A, 0x02};   /* fake sync */
    memcpy(stream, junk, sizeof junk);
    n += sizeof junk;
    n += make_nav_frame(7u, 100.0f, &stream[n]);
    n += make_nav_frame(9u, 200.0f, &stream[n]);              /* seq 8 lost */
    n += make_nav_frame(8u, 150.0f, &stream[n]);              /* ...arrives late */
    RnParser p;
    rn_parser_init(&p);
    RnPacket pkt;
    int packets = 0;
    float last_along = 0.0f;
    for (uint32_t i = 0u; i < n; i++) {
        RnParseResult res = rn_parser_push(&p, stream[i], &pkt);
        for (int d = 0; d < 4 && res != RN_PARSE_PACKET; d++) {
            res = rn_parser_drain(&p, &pkt);
            if (res != RN_PARSE_PACKET) {
                break;
            }
        }
        if (res == RN_PARSE_PACKET) {
            RnNavState s;
            CHECK(rn_nav_state_decode(&pkt, &s) == RN_OK);
            CHECK(s.route_id == 0xCAFEu);
            CHECK_NEAR(s.lat, 17.4455123, 1e-6);
            CHECK_NEAR(s.speed_mps, 11.5f, 0.01f);
            CHECK(s.eta_s == 61u);
            last_along = s.along_m;
            packets++;
        }
    }
    CHECK(packets == 3);
    CHECK_NEAR(last_along, 150.0f, 0.001f);
    CHECK(p.dropped == 0u);                                   /* late packet gave it back */
    CHECK(p.ok == 3u);
}

static void test_corruption(void)
{
    uint8_t frame[RN_PKT_MAX_FRAME];
    uint32_t n = make_nav_frame(1u, 10.0f, frame);
    frame[12] ^= 0x40u;
    RnParser p;
    rn_parser_init(&p);
    RnPacket pkt;
    int got = 0;
    for (uint32_t i = 0u; i < n; i++) {
        got += rn_parser_push(&p, frame[i], &pkt) == RN_PARSE_PACKET;
    }
    CHECK(got == 0);
    CHECK(p.crc_errors == 1u);
}

static void test_map_chunk(void)
{
    RnMapChunk c;
    c.offset = 4800u;
    c.n = 240u;
    for (uint32_t i = 0u; i < 240u; i++) {
        c.data[i] = (uint8_t)i;
    }
    RnPacket pkt;
    memset(&pkt, 0, sizeof pkt);
    pkt.type = RN_PKT_MAP_CHUNK;
    CHECK(rn_map_chunk_encode(&c, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK);
    CHECK(pkt.len == 246u);
    RnMapChunk d;
    CHECK(rn_map_chunk_decode(&pkt, &d) == RN_OK);
    CHECK(d.offset == 4800u && d.n == 240u && d.data[239] == 239u);
    pkt.len = 100u;                                           /* length mismatch */
    CHECK(rn_map_chunk_decode(&pkt, &d) == RN_ERR_FORMAT);
}

static void test_map_begin(void)
{
    RnMapBegin b = {683208u, 0x441F5F8Du, 0x58AE9690u, 401234u, 0xCAFEF00Du};
    RnPacket pkt;
    memset(&pkt, 0, sizeof pkt);
    pkt.type = RN_PKT_MAP_BEGIN;
    CHECK(rn_map_begin_encode(&b, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK);
    CHECK(pkt.len == 20u);
    RnMapBegin d;
    CHECK(rn_map_begin_decode(&pkt, &d) == RN_OK);
    CHECK(d.total_len == 683208u && d.crc32 == 0x441F5F8Du && d.route_id == 0x58AE9690u && d.wire_len == 401234u);
    CHECK(d.base_crc == 0xCAFEF00Du);
    pkt.len = 16u;                                            /* phones before patches */
    CHECK(rn_map_begin_decode(&pkt, &d) == RN_OK && d.base_crc == 0u && d.wire_len == 401234u);
    pkt.len = 12u;                                            /* old phones: raw transfer */
    CHECK(rn_map_begin_decode(&pkt, &d) == RN_OK && d.wire_len == 0u && d.total_len == 683208u);
    pkt.len = 8u;
    CHECK(rn_map_begin_decode(&pkt, &d) == RN_ERR_FORMAT);
}

static void test_notify(void)
{
    RnNotify n;
    memset(&n, 0, sizeof n);
    n.t_ms = 99u;
    n.id = 7u;
    n.kind = RN_NOTIFY_MESSAGE;
    (void)strcpy(n.app, "WhatsApp");
    (void)strcpy(n.title, "Amma \xf0\x9f\x98\x8a");            /* emoji dropped */
    static const char LONG[] = "Where are you? Call me when you reach, it is getting late and the "
                               "food is ready. Also bring milk on the way home please, thanks!!";
    (void)memcpy(n.text, LONG, RN_NOTIFY_TEXT_MAX);           /* longer than the field: cut */
    n.text[RN_NOTIFY_TEXT_MAX] = '\0';
    RnPacket pkt;
    memset(&pkt, 0, sizeof pkt);
    pkt.type = RN_PKT_NOTIFY;
    CHECK(rn_notify_encode(&n, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK);
    CHECK(pkt.len == 168u);
    RnNotify d;
    CHECK(rn_notify_decode(&pkt, &d) == RN_OK);
    CHECK(d.id == 7u && d.kind == RN_NOTIFY_MESSAGE && strcmp(d.app, "WHATSAPP") == 0 && strcmp(d.title, "AMMA") == 0);
    CHECK(strncmp(d.text, "WHERE ARE YOU? CALL ME", 22) == 0 && strlen(d.text) == RN_NOTIFY_TEXT_MAX);
    pkt.payload[6] = 9u;                                      /* unknown kind */
    CHECK(rn_notify_decode(&pkt, &d) == RN_ERR_FORMAT);
}

static void test_route_chunk(void)
{
    RnRouteChunk c;
    memset(&c, 0, sizeof c);
    c.route_id = 0x1234u;
    c.pack_route_id = 0xBEEFu;
    c.total = 40u;
    c.first = 36u;
    c.n = 4u;
    c.total_m = 3100.5f;
    for (uint32_t i = 0u; i < 4u; i++) {
        c.x[i] = 10.0f * (float)i - 5.0f;
        c.y[i] = -2.5f * (float)i;
        c.along[i] = 3000.0f + 25.0f * (float)i;
    }
    RnPacket pkt;
    memset(&pkt, 0, sizeof pkt);
    pkt.type = RN_PKT_ROUTE;
    CHECK(rn_route_chunk_encode(&c, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK);
    CHECK(pkt.len == 17u + 4u * 12u);
    RnRouteChunk d;
    CHECK(rn_route_chunk_decode(&pkt, &d) == RN_OK);
    CHECK(d.route_id == 0x1234u && d.pack_route_id == 0xBEEFu && d.total == 40u && d.first == 36u && d.n == 4u);
    CHECK(d.x[3] == 25.0f && d.y[2] == -5.0f && d.along[1] == 3025.0f);
    pkt.payload[12] = 5u;                                     /* n says 5, payload has 4 points */
    CHECK(rn_route_chunk_decode(&pkt, &d) == RN_ERR_FORMAT);
    pkt.payload[12] = 4u;
    pkt.payload[10] = 38u;                                    /* first 38 + 4 > total 40 */
    CHECK(rn_route_chunk_decode(&pkt, &d) == RN_ERR_FORMAT);
    /* the full 18-point chunk fits a frame */
    c.first = 0u;
    c.n = RN_ROUTE_CHUNK_PTS;
    CHECK(rn_route_chunk_encode(&c, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK && pkt.len <= RN_PKT_MAX_PAYLOAD);
}

static void test_line(void)
{
    static const RnPackPoint pts[3] = {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 100.0f}, {10.0f, 20.0f, 300.0f}};
    RnLine l = rn_line_from_points(pts, 3u);
    CHECK(rn_line_count(&l) == 3u && rn_line_total(&l) == 300.0f);
    RnPackPoint a = rn_line_position_at(&l, 50.0f);
    CHECK_NEAR(a.x, 5.0f, 1e-4f);
    RnPackPoint b = rn_line_position_at(&l, 200.0f);
    CHECK_NEAR(b.x, 10.0f, 1e-4f);
    CHECK_NEAR(b.y, 10.0f, 1e-4f);
    CHECK(rn_line_position_at(&l, 999.0f).y == 20.0f && rn_line_position_at(&l, -5.0f).x == 0.0f);
}

void test_protocol_calls(void);

void test_protocol(void)
{
    (void)printf("protocol\n");
    test_crc();
    test_roundtrip_with_garbage();
    test_corruption();
    test_map_chunk();
    test_map_begin();
    test_notify();
    test_route_chunk();
    test_line();
    test_protocol_calls();
}

void test_protocol_calls(void)
{
    RnPacket pkt;
    memset(&pkt, 0, sizeof pkt);
    RnForecast f;
    memset(&f, 0, sizeof f);
    f.t_ms = 99u;
    f.start_hour = 18u;
    f.n = 6u;
    f.sunrise_min = 6u * 60u + 5u;
    f.sunset_min = 17u * 60u + 59u;
    for (uint32_t i = 0u; i < 6u; i++) {
        f.hours[i].temp_c = 30.0f - (float)i;
        f.hours[i].code = (uint8_t)(i == 3u ? 61u : 2u);
        f.hours[i].rain_prob = (uint8_t)(10u * i);
        f.hours[i].wind_kmh = 9u;
    }
    pkt.type = RN_PKT_FORECAST;
    CHECK(rn_forecast_encode(&f, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK && pkt.len == 40u);
    RnForecast g;
    CHECK(rn_forecast_decode(&pkt, &g) == RN_OK);
    CHECK(g.n == 6u && g.start_hour == 18u && g.sunset_min == 1079u && g.hours[3].code == 61u);
    CHECK_NEAR(g.hours[5].temp_c, 25.0f, 0.01f);
    pkt.len = 20u;
    CHECK(rn_forecast_decode(&pkt, &g) == RN_ERR_FORMAT);

    RnCallState c;
    memset(&c, 0, sizeof c);
    c.call_id = 7u;
    c.state = RN_CALL_RINGING;
    (void)strcpy(c.name, "Amma \xf0\x9f\x92\x9c");
    pkt.type = RN_PKT_CALL_STATE;
    CHECK(rn_call_state_encode(&c, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK && pkt.len == 32u);
    RnCallState d;
    CHECK(rn_call_state_decode(&pkt, &d) == RN_OK);
    CHECK(d.call_id == 7u && d.state == RN_CALL_RINGING && strcmp(d.name, "AMMA") == 0);

    RnCallAction a = {7u, RN_ACTION_REJECT_REPLY};
    pkt.type = RN_PKT_CALL_ACTION;
    CHECK(rn_call_action_encode(&a, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK);
    RnCallAction b;
    CHECK(rn_call_action_decode(&pkt, &b) == RN_OK && b.call_id == 7u && b.action == RN_ACTION_REJECT_REPLY);

    RnPrepStatus ps = {1234u, RN_PREP_MAP, 62u, 23u, 37u};
    pkt.type = RN_PKT_PREP_STATUS;
    CHECK(rn_prep_status_encode(&ps, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK && pkt.len == 10u);
    RnPrepStatus pd;
    CHECK(rn_prep_status_decode(&pkt, &pd) == RN_OK);
    CHECK(pd.t_ms == 1234u && pd.stage == RN_PREP_MAP && pd.pct == 62u && pd.done == 23u && pd.total == 37u);
    pkt.payload[4] = 9u;                                       /* unknown stage */
    CHECK(rn_prep_status_decode(&pkt, &pd) == RN_ERR_FORMAT);
    RnGuidance gd;
    memset(&gd, 0, sizeof gd);
    gd.maneuver = RN_M_RIGHT;
    gd.flags = RN_GUIDE_ACTIVE;
    gd.to_turn_m = 250u;
    gd.remaining_m = 4200u;
    gd.eta_s = 64680u;
    (void)strcpy(gd.road, "Old Mumbai Hwy");
    pkt.type = RN_PKT_GUIDANCE;
    CHECK(rn_guidance_encode(&gd, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK && pkt.len == 48u);
    RnGuidance ge;
    CHECK(rn_guidance_decode(&pkt, &ge) == RN_OK);
    CHECK(ge.maneuver == RN_M_RIGHT && ge.to_turn_m == 250u && ge.remaining_m == 4200u && ge.eta_s == 64680u);
    CHECK(strcmp(ge.road, "OLD MUMBAI HWY") == 0 && (ge.flags & RN_GUIDE_ACTIVE) != 0u);
    pkt.payload[4] = 200u;                                     /* unknown maneuver */
    CHECK(rn_guidance_decode(&pkt, &ge) == RN_ERR_FORMAT);

    pkt.type = RN_PKT_PREP_STATUS;
    ps.done = 40u;                                             /* done > total: clamped */
    CHECK(rn_prep_status_encode(&ps, pkt.payload, sizeof pkt.payload, &pkt.len) == RN_OK);
    CHECK(rn_prep_status_decode(&pkt, &pd) == RN_OK && pd.done == pd.total);
    pkt.payload[8] = 10u;                                      /* total 10 < done 37: still refused */
    pkt.payload[9] = 0u;
    CHECK(rn_prep_status_decode(&pkt, &pd) == RN_ERR_FORMAT);
}
