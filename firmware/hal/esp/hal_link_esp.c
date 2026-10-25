/*
 * hal_link_esp.c - the phone link over BLE (NimBLE host), Nordic-UART style
 * GATT service (UUIDs in docs/PROTOCOL.md):
 *
 *   RX characteristic  phone writes protocol bytes (write / write no response)
 *   TX characteristic  ROTOS notifies protocol bytes (ACK, BUTTON, PING)
 *
 * Received bytes go through a static FreeRTOS stream buffer from the NimBLE
 * host task to the ROTOS loop. NimBLE requires callbacks (function
 * pointers); per the coding standard they are confined to this file.
 */
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "hal_link.h"
#include "hal_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define RX_BUFFER 12288u                   /* > the app's 40-chunk map window (10 KB) */
#define DEVICE_NAME "ROTOS"

/* 6e40000X-b5a3-f393-e0a9-e50e24dcca9e, little-endian byte order */
static const ble_uuid128_t SVC_UUID = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                                                       0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t RX_UUID = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                                                      0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t TX_UUID = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                                                      0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static StaticStreamBuffer_t s_rx_struct;
static uint8_t s_rx_storage[RX_BUFFER + 1u];
static StreamBufferHandle_t s_rx;
static uint16_t s_tx_handle;
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static uint8_t s_addr_type;

static int gatt_access(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t tmp[512];
    uint16_t n = 0u;
    if (ble_hs_mbuf_to_flat(ctxt->om, tmp, sizeof tmp, &n) != 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    (void)xStreamBufferSend(s_rx, tmp, n, 0);       /* full: drop, protocol recovers */
    return 0;
}

static const struct ble_gatt_svc_def SERVICES[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SVC_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {.uuid = &RX_UUID.u, .access_cb = gatt_access, .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
            {.uuid = &TX_UUID.u, .access_cb = gatt_access, .val_handle = &s_tx_handle, .flags = BLE_GATT_CHR_F_NOTIFY},
            {0},
        },
    },
    {0},
};

static int gap_event(struct ble_gap_event *ev, void *arg);

static void advertise(void)
{
    struct ble_hs_adv_fields f;
    (void)memset(&f, 0, sizeof f);
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.name = (const uint8_t *)DEVICE_NAME;
    f.name_len = (uint8_t)strlen(DEVICE_NAME);
    f.name_is_complete = 1;
    f.uuids128 = &SVC_UUID;
    f.num_uuids128 = 1;
    f.uuids128_is_complete = 1;
    struct ble_gap_adv_params p;
    (void)memset(&p, 0, sizeof p);
    p.conn_mode = BLE_GAP_CONN_MODE_UND;
    p.disc_mode = BLE_GAP_DISC_MODE_GEN;
    if (ble_gap_adv_set_fields(&f) != 0 ||
        ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &p, gap_event, NULL) != 0) {
        hal_log("ble", "advertising failed", 0);
    }
}

/* Faster map uploads: 251-byte link-layer packets (BLE 4.2 data length
 * extension) and a short connection interval (7.5-15 ms) while connected. */
static void speed_up(uint16_t conn)
{
    struct ble_gap_upd_params p;
    (void)memset(&p, 0, sizeof p);
    p.itvl_min = 6u;                       /* x 1.25 ms: 7.5 ms, the fastest BLE allows */
    p.itvl_max = 6u;
    p.latency = 0u;
    p.supervision_timeout = 600u;          /* x 10 ms: a phone deep in a pocket gets 6 s */
    int dl = ble_gap_set_data_len(conn, 251u, 2120u);
    int up = ble_gap_update_params(conn, &p);
    hal_log("ble", "speed-up (data len rc, conn params rc x100)", dl + up * 100);
}

static int gap_event(struct ble_gap_event *ev, void *arg)
{
    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        s_conn = (ev->connect.status == 0) ? ev->connect.conn_handle : BLE_HS_CONN_HANDLE_NONE;
        hal_log("ble", (ev->connect.status == 0) ? "phone connected" : "connect failed", ev->connect.status);
        if (ev->connect.status != 0) {
            advertise();
        } else {
            speed_up(ev->connect.conn_handle);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        s_conn = BLE_HS_CONN_HANDLE_NONE;
        hal_log("ble", "phone disconnected, reason", ev->disconnect.reason);
        advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        break;
    case BLE_GAP_EVENT_MTU:
        hal_log("ble", "MTU", ev->mtu.value);
        break;
    default:
        break;
    }
    return 0;
}

static volatile bool s_synced;              /* NimBLE host ready to advertise */

static void on_sync(void)
{
    s_synced = true;
    (void)ble_hs_util_ensure_addr(0);
    (void)ble_hs_id_infer_auto(0, &s_addr_type);
    advertise();
    hal_log("ble", "advertising as " DEVICE_NAME, 0);
}

static void host_task(void *param)
{
    nimble_port_run();                     /* returns only on nimble_port_stop() */
    nimble_port_freertos_deinit();
}

bool hal_link_init(void)
{
    s_rx = xStreamBufferCreateStatic(RX_BUFFER, 1, s_rx_storage, &s_rx_struct);
    if (s_rx == NULL || nimble_port_init() != ESP_OK) {
        return false;
    }
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    if (ble_gatts_count_cfg(SERVICES) != 0 || ble_gatts_add_svcs(SERVICES) != 0) {
        return false;
    }
    (void)ble_svc_gap_device_name_set(DEVICE_NAME);
    nimble_port_freertos_init(host_task);
    return true;
}

/* Not connected and not advertising (a start that failed while the old link
 * was still closing): try again, at most once a second, so ROTOS never
 * stays invisible until a power cycle. */
static void keep_advertising(void)
{
    static uint32_t last_ms;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (!s_synced || s_conn != BLE_HS_CONN_HANDLE_NONE || ble_gap_adv_active() || now - last_ms < 1000u) {
        return;
    }
    last_ms = now;
    hal_log("ble", "advertising again", 0);
    advertise();
}

uint32_t hal_link_read(uint8_t *buf, uint32_t cap)
{
    keep_advertising();
    if (buf == NULL || cap == 0u || s_rx == NULL) {
        return 0u;
    }
    return (uint32_t)xStreamBufferReceive(s_rx, buf, cap, 0);
}

bool hal_link_write(const uint8_t *bytes, uint32_t n)
{
    uint16_t conn = s_conn;
    if (bytes == NULL || conn == BLE_HS_CONN_HANDLE_NONE) {
        return false;
    }
    uint16_t mtu = ble_att_mtu(conn);
    uint32_t step = (mtu > 3u) ? (uint32_t)mtu - 3u : 20u;
    for (uint32_t off = 0u; off < n; off += step) {
        uint32_t k = (n - off < step) ? n - off : step;
        struct os_mbuf *om = ble_hs_mbuf_from_flat(&bytes[off], (uint16_t)k);
        if (om == NULL || ble_gatts_notify_custom(conn, s_tx_handle, om) != 0) {
            return false;
        }
    }
    return true;
}

bool hal_link_connected(void)
{
    return s_conn != BLE_HS_CONN_HANDLE_NONE;
}
