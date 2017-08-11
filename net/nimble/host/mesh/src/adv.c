/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mesh.h"

#define BT_DBG_ENABLED (MYNEWT_VAL(BLE_MESH_DEBUG_ADV))
#include "host/ble_hs_log.h"
#include "host/ble_hs_adv.h"
#include "host/ble_gap.h"
#include "nimble/hci_common.h"

#include "adv.h"
#include "foundation.h"
#include "net.h"
#include "beacon.h"
#include "prov.h"
#include "proxy.h"

/* Window and Interval are equal for continuous scanning */
#define MESH_SCAN_INTERVAL 0x10
#define MESH_SCAN_WINDOW   0x10

/* Convert from ms to 0.625ms units */
#define ADV_INT(_ms) ((_ms) * 8 / 5)

/* Pre-5.0 controllers enforce a minimum interval of 100ms
 * whereas 5.0+ controllers can go down to 20ms.
 */
#define ADV_INT_DEFAULT  K_MSEC(100)
#define ADV_INT_FAST     K_MSEC(20)

/* TinyCrypt PRNG consumes a lot of stack space, so we need to have
 * an increased call stack whenever it's used.
 */
#if (MYNEWT_VAL(BLE_HOST_CRYPTO))
#define ADV_STACK_SIZE 768
#else
#define ADV_STACK_SIZE 512
#endif

struct os_task adv_task;
static struct os_eventq adv_queue;
static uint8_t own_addr_type;

/*XX USER DATA*/
static os_membuf_t adv_buf_mem[OS_MEMPOOL_SIZE(
        MYNEWT_VAL(BLE_MESH_ADV_BUF_COUNT),
        BT_MESH_ADV_DATA_SIZE + sizeof(struct os_mbuf))];

struct os_mbuf_pool adv_os_mbuf_pool;
static struct os_mempool adv_buf_mempool;

static const u8_t adv_type[] = {
    [BT_MESH_ADV_PROV] = BLE_HS_ADV_TYPE_MESH_PROV,
    [BT_MESH_ADV_DATA] = BLE_HS_ADV_TYPE_MESH_MESSAGE,
    [BT_MESH_ADV_BEACON] = BLE_HS_ADV_TYPE_MESH_BEACON,
};


static inline void adv_sent(struct net_buf *buf, int err)
{
	if (BT_MESH_ADV(buf)->busy) {
		BT_MESH_ADV(buf)->busy = 0;

		if (BT_MESH_ADV(buf)->sent) {
			BT_MESH_ADV(buf)->sent(buf, err);
		}
	}

	net_buf_unref(buf);
}

static inline void adv_send(struct net_buf *buf)
{
    /*(bt_dev.hci_version >= BT_HCI_VERSION_5_0) ? */
    const s32_t adv_int_min = ADV_INT_FAST;
    struct ble_gap_adv_params param = { 0 };
    u16_t duration, adv_int;
    struct bt_mesh_adv *adv = BT_MESH_ADV(buf);
    int err;

    adv_int = max(adv_int_min, adv->adv_int);
    duration = (adv->count + 1) * (adv_int + 10);

    BT_DBG("type %u len %u:", adv->type,
           buf->om_len);
    BT_DBG("count %u interval %ums duration %ums",
               adv->count + 1, adv_int, duration);

    /* To verify: Add type and len */
    buf = os_mbuf_prepend(buf, 2);
    buf->om_data[0] = OS_MBUF_PKTLEN(buf);
    buf->om_data[1] = adv_type[BT_MESH_ADV(buf)->type];
    buf = os_mbuf_pullup(buf, OS_MBUF_PKTLEN(buf));
    assert(buf);
    ble_gap_adv_set_data(buf->om_data, buf->om_len);

    param.itvl_min = ADV_INT(adv_int);
    param.itvl_max = param.itvl_min;
    param.conn_mode = BLE_GAP_CONN_MODE_NON;

    err = ble_gap_adv_start(0x00, NULL, duration, &param, NULL, NULL);
    adv_sent(buf, err);
    if (err) {
        BT_ERR("Advertising failed: err %d", err);
        return;
    }
    BT_DBG("Advertising started with duration %u ms", duration);
}

void
bt_mesh_adv_evt_cb(struct os_event *ev)
{
    struct os_mbuf *adv_data = ev->ev_arg;
    struct bt_mesh_adv *adv = BT_MESH_ADV(adv_data);

    if (!adv_data) {
#if (MYNEWT_VAL(BLE_MESH_PROXY))
        if (ble_gap_adv_active()) {
            bt_mesh_proxy_adv_stop();
        }
#endif
        return;
    }

#if (MYNEWT_VAL(BLE_MESH_PROXY))
    if (!ble_gap_adv_active())
  {
        s32_t timeout;

        timeout = bt_mesh_proxy_adv_start();
        BT_DBG("Proxy Advertising up to %d ms", timeout);
        // COLLOUT
        return;
    }
#endif

    /* busy == 0 means this was canceled */
    if (adv->busy) {
        adv_send(adv_data);
    }
}

static void
adv_thread(void *args)
{
    BT_DBG("started");

    while (1) {
        os_eventq_run(&adv_queue);
    }
}

void bt_mesh_adv_update(void)
{
	BT_DBG("");

	//k_fifo_cancel_wait(&adv_queue);
}

struct net_buf *bt_mesh_adv_create(enum bt_mesh_adv_type type, u8_t xmit_count,
				   u8_t xmit_int, s32_t timeout)
{
    struct os_mbuf *adv_data;
    struct bt_mesh_adv *adv;

    adv_data = os_mbuf_get_pkthdr(&adv_os_mbuf_pool, sizeof(struct bt_mesh_adv));
    if (!adv_data) {
        return NULL;
    }

    adv = BT_MESH_ADV(adv_data);
    memset(adv, 0, sizeof(*adv));

    adv->type = type;
    adv->count = xmit_count;
    adv->adv_int = xmit_int;
    adv->mg_ref = 1;
    adv->ev.ev_arg = adv_data;
    return adv_data;
}

void bt_mesh_adv_send(struct net_buf *buf, bt_mesh_adv_func_t sent)
{
	BT_DBG("type 0x%02x len %u: %s", BT_MESH_ADV(buf)->type, buf->om_len,
	       bt_hex(buf->om_data, buf->om_len));

	BT_MESH_ADV(buf)->sent = sent;
	BT_MESH_ADV(buf)->busy = 1;
	BT_MESH_ADV(buf)->ev.ev_cb = bt_mesh_adv_evt_cb;

	net_buf_put(&adv_queue, net_buf_ref(buf));
}

static void bt_mesh_scan_cb(const bt_addr_le_t *addr, s8_t rssi,
                            u8_t adv_type, struct net_buf_simple *buf)
{
	if (adv_type != BLE_HCI_ADV_TYPE_ADV_NONCONN_IND) {
		return;
	}

	BT_DBG("len %u: %s", buf->om_len, bt_hex(buf->om_data, buf->om_len));

	while (buf->om_len > 1) {
		struct net_buf_simple_state state;
		u8_t len, type;

		len = net_buf_simple_pull_u8(buf);
		/* Check for early termination */
		if (len == 0) {
			return;
		}

		if (len > buf->om_len || buf->om_len < 1) {
			BT_WARN("AD malformed");
			return;
		}

		net_buf_simple_save(buf, &state);

		type = net_buf_simple_pull_u8(buf);

		buf->om_len = len - 1;

		switch (type) {
		case BLE_HS_ADV_TYPE_MESH_MESSAGE:
			bt_mesh_net_recv(buf, rssi, BT_MESH_NET_IF_ADV);
			break;
#if MYNEWT_VAL(BLE_MESH_PB_ADV)
		case BLE_HS_ADV_TYPE_MESH_PROV:
			bt_mesh_pb_adv_recv(buf);
			break;
#endif
		case BLE_HS_ADV_TYPE_MESH_BEACON:
			bt_mesh_beacon_recv(buf);
			break;
		default:
			break;
		}

		net_buf_simple_restore(buf, &state);
		net_buf_simple_pull(buf, len);
	}
}

void bt_mesh_adv_init(uint8_t own_addr_type)
{
    os_stack_t *pstack;
    int rc;

    pstack = malloc(sizeof(os_stack_t) * ADV_STACK_SIZE);
    assert(pstack);

    rc = os_mempool_init(&adv_buf_mempool, MYNEWT_VAL(BLE_MESH_ADV_BUF_COUNT),
    BT_MESH_ADV_DATA_SIZE + BT_MESH_ADV_USER_DATA_SIZE,
                         adv_buf_mem, "adv_buf_pool");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&adv_os_mbuf_pool, &adv_buf_mempool,
                           BT_MESH_ADV_DATA_SIZE + BT_MESH_ADV_USER_DATA_SIZE,
                           MYNEWT_VAL(BLE_MESH_ADV_BUF_COUNT));
    assert(rc == 0);

    os_task_init(&adv_task, "mesh_adv", adv_thread, NULL,
                 MYNEWT_VAL(BLE_MESH_ADV_TASK_PRIO), OS_WAIT_FOREVER, pstack,
                 ADV_STACK_SIZE);

    os_eventq_init(&adv_queue);
}

static int
ble_adv_gap_mesh_cb(struct ble_gap_event *event, void *arg)
{
#if MYNEWT_VAL(BLE_EXT_ADV)
    struct ble_gap_ext_disc_desc *ext_desc;
#endif
    struct ble_gap_disc_desc *desc;
    struct os_mbuf *buf;

    switch (event->type) {
#if MYNEWT_VAL(BLE_EXT_ADV)
        case BLE_GAP_EVENT_EXT_DISC:
            ext_desc = &event->ext_disc;
            buf = os_mbuf_get_pkthdr(&adv_os_mbuf_pool, 0);
            os_mbuf_append(buf, ext_desc->data, ext_desc->length_data);
            bt_mesh_scan_cb(&ext_desc->addr, ext_desc->rssi,
                            ext_desc->legacy_event_type, buf);
            break;
#endif
        case BLE_GAP_EVENT_DISC:
            desc = &event->disc;
            buf = os_mbuf_get_pkthdr(&adv_os_mbuf_pool, 0);
            os_mbuf_append(buf, desc->data, desc->length_data);

            bt_mesh_scan_cb(&desc->addr, desc->rssi, desc->event_type, buf);
            break;
        default:
            break;
    }

    return 0;
}

int bt_mesh_scan_enable(void)
{
    struct ble_gap_disc_params scan_param =
        { .passive = 1, .filter_duplicates = 0, .itvl =
        MESH_SCAN_INTERVAL, .window = MESH_SCAN_WINDOW };

    BT_DBG("");

    ble_gap_mesh_register(ble_adv_gap_mesh_cb, NULL);
    return ble_gap_disc(own_addr_type, 0, &scan_param, NULL, NULL);
}

int bt_mesh_scan_disable(void)
{
	BT_DBG("");

	return ble_gap_disc_cancel();
}
