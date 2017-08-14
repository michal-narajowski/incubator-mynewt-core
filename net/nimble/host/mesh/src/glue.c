/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "glue.h"
#include "adv.h"
#include "../src/ble_hs_conn_priv.h"

const char *
bt_hex(const void *buf, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    static char hexbufs[4][129];
    static u8_t curbuf;
    const u8_t *b = buf;
    //unsigned int mask;
    char *str;
    int i;

    //mask = irq_lock();
    str = hexbufs[curbuf++];
    curbuf %= ARRAY_SIZE(hexbufs);
    //irq_unlock(mask);

    len = min(len, (sizeof(hexbufs[0]) - 1) / 2);

    for (i = 0; i < len; i++) {
        str[i * 2] = hex[b[i] >> 4];
        str[i * 2 + 1] = hex[b[i] & 0xf];
    }

    str[i * 2] = '\0';

    return str;
}

void
net_buf_put(struct k_fifo *fifo, struct net_buf *buf)
{
    struct os_event *ev = &BT_MESH_ADV(buf)->ev;
    assert(OS_MBUF_IS_PKTHDR(buf));


    os_eventq_put(fifo, ev);
}

void *
net_buf_ref(struct os_mbuf *om)
{
    struct bt_mesh_adv *adv;

    /* For bufs with header we count refs*/
    if (OS_MBUF_USRHDR_LEN(om) == 0) {
        return NULL;
    }

    adv = BT_MESH_ADV(om);
    adv->mg_ref++;

    return om;
}

void
net_buf_unref(struct os_mbuf *om)
{
    struct bt_mesh_adv *adv;

    /* For bufs with header we count refs*/
    if (OS_MBUF_USRHDR_LEN(om) == 0) {
        goto free;
    }

    adv = BT_MESH_ADV(om);
    if (--adv->mg_ref > 0) {
        return;
    }

free:
    os_mbuf_free_chain(om);
}

int
bt_encrypt_le(const uint8_t *key, const uint8_t *plaintext, uint8_t *enc_data)
{
    struct tc_aes_key_sched_struct s;
    uint8_t tmp[16];

    swap_buf(tmp, key, 16);

    if (tc_aes128_set_encrypt_key(&s, tmp) == TC_CRYPTO_FAIL) {
        return BLE_HS_EUNKNOWN;
    }

    swap_buf(tmp, plaintext, 16);

    if (tc_aes_encrypt(enc_data, tmp, &s) == TC_CRYPTO_FAIL) {
        return BLE_HS_EUNKNOWN;
    }

    swap_in_place(enc_data, 16);

    return 0;
}

int
bt_encrypt_be(const uint8_t *key, const uint8_t *plaintext, uint8_t *enc_data)
{
    struct tc_aes_key_sched_struct s;
    uint8_t tmp[16];

    if (tc_aes128_set_encrypt_key(&s, tmp) == TC_CRYPTO_FAIL) {
        return BLE_HS_EUNKNOWN;
    }

    if (tc_aes_encrypt(enc_data, tmp, &s) == TC_CRYPTO_FAIL) {
        return BLE_HS_EUNKNOWN;
    }

    return 0;
}

uint16_t
bt_gatt_get_mtu(struct ble_hs_conn *conn)
{
    return ble_att_mtu(conn->bhc_handle);
}

/* os_mbuf helpers */
uint16_t
net_buf_simple_pull_le16(struct os_mbuf *om)
{
    uint16_t val;

    val = get_le16(om->om_data);
    os_mbuf_adj(om, sizeof(val));

    return val;
}

uint16_t
net_buf_simple_pull_be16(struct os_mbuf *om)
{
    uint16_t val;

    val = get_be16(om->om_data);
    os_mbuf_adj(om, sizeof(val));

    return val;
}

uint32_t
net_buf_simple_pull_be32(struct os_mbuf *om)
{
    uint32_t val;

    val = get_be32(om->om_data);
    os_mbuf_adj(om, sizeof(val));

    return val;
}

uint8_t
net_buf_simple_pull_u8(struct os_mbuf *om)
{
    uint8_t val;

    val = om->om_data[0];
    os_mbuf_adj(om, 1);

    return val;
}

void
net_buf_simple_add_le16(struct os_mbuf *om, uint16_t val)
{
    val = htole16(val);
    os_mbuf_append(om, &val, sizeof(val));
}

void
net_buf_simple_add_be16(struct os_mbuf *om, uint16_t val)
{
    val = htobe16(val);
    os_mbuf_append(om, &val, sizeof(val));
}

void
net_buf_simple_add_be32(struct os_mbuf *om, uint32_t val)
{
    val = htobe32(val);
    os_mbuf_append(om, &val, sizeof(val));
}

void
net_buf_simple_add_u8(struct os_mbuf *om, uint8_t val)
{
    os_mbuf_append(om, &val, 1);
}

void
net_buf_simple_push_le16(struct os_mbuf *om, uint16_t val)
{
    om = os_mbuf_prepend(om, sizeof(val));
    om = os_mbuf_pullup(om, OS_MBUF_PKTLEN(om));
    put_le16(om->om_data, val);
}

void
net_buf_simple_push_be16(struct os_mbuf *om, uint16_t val)
{
    om = os_mbuf_prepend(om, sizeof(val));
    om = os_mbuf_pullup(om, OS_MBUF_PKTLEN(om));
    put_be16(om->om_data, val);
}

void
net_buf_simple_push_u8(struct os_mbuf *om, uint8_t val)
{
    om = os_mbuf_prepend(om, 1);
    om = os_mbuf_pullup(om, OS_MBUF_PKTLEN(om));
    om->om_data[0] = val;
}

void
net_buf_add_zeros(struct os_mbuf *om, uint8_t len)
{
    uint8_t z[len];

    memset(z, 0, len);

    os_mbuf_append(om, z, len);
}

void *
net_buf_simple_pull(struct os_mbuf *om, uint8_t len)
{
    os_mbuf_adj(om, len);
    return om->om_data;
}

void*
net_buf_add(struct os_mbuf *om, uint8_t len)
{
    void *old_tail = om->om_data;

    om = os_mbuf_extend(om, len);

    return old_tail;
}

bool
k_fifo_is_empty(struct os_eventq *q)
{
    return STAILQ_EMPTY(&q->evq_list);
}

void * net_buf_get(struct k_fifo *fifo,s32_t t)
{
    struct os_event *ev = os_eventq_get_no_wait(fifo);

    return ev->ev_arg;
}

uint8_t *
net_buf_simple_push(struct os_mbuf *om, uint8_t len)
{
    os_mbuf_prepend(om, len);
    return om->om_data;
}

void
net_buf_reserve(struct net_buf *buf, size_t reserve)
{
    /* Add empty buf which will be later overwitten */
    net_buf_add_zeros(buf, reserve);
}

void
k_work_init(struct k_work *work, k_work_handler_t handler)
{
    os_callout_init(work, os_eventq_dflt_get(), handler, NULL);
}

void
k_delayed_work_init(struct k_delayed_work *w, os_event_fn *f)
{
    os_callout_init(&w->work, os_eventq_dflt_get(), f, NULL);
}

void
k_delayed_work_cancel(struct k_delayed_work *w)
{
    os_callout_stop(&w->work);
}

void
k_delayed_work_submit(struct k_delayed_work *w, uint32_t ms)
{
    uint32_t ticks;

    os_time_ms_to_ticks(ms, &ticks);
    os_callout_reset(&w->work, ticks);
}

void
k_work_submit(struct k_work *w)
{
    os_callout_reset(w, 0);
}

void
k_work_add_arg(struct k_work *w, void *arg)
{
    w->c_ev.ev_arg = arg;
}

uint32_t
k_delayed_work_remaining_get (struct k_delayed_work *w)
{
    return os_callout_remaining_ticks(&w->work, os_cputime_get32());
}

int64_t k_uptime_get(void)
{
    /*FIXME*/
    return os_cputime_get32() * OS_TICKS_PER_SEC * 1000;
}

u32_t k_uptime_get_32(void)
{
    return k_uptime_get();
}

int
bt_dh_key_gen(const u8_t remote_pk[64], bt_dh_key_cb_t cb)
{
    uint32_t our_priv_key;
    uint8_t dh[32];

    if (!ble_sm_alg_gen_dhkey((uint8_t *)&remote_pk[0], (uint8_t *)&remote_pk[32],
                              &our_priv_key, dh)) {
        return -1;
    }

    cb(dh);
    return 0;
}

int
bt_rand(void *buf, size_t len)
{
    int rc;
    rc = ble_hs_hci_util_rand(buf, len);
    if (rc != 0) {
        return -1;
    }

    return 0;
}

static uint8_t pub[64];
static bool has_pub = false;

int
bt_pub_key_gen(struct bt_pub_key_cb *new_cb)
{
    uint32_t priv[8];

    /* ?? */
    if (ble_sm_alg_gen_key_pair(pub, priv)) {
        assert(0);
        return -1;
    }

    new_cb->func(pub);
    has_pub = true;

    return 0;
}

uint8_t *
bt_pub_key_get(void)
{
    if (!has_pub) {
        return NULL;
    }

    return pub;
}

static int
set_ad(const struct bt_data *ad, size_t ad_len, u8_t *buf, u8_t *buf_len)
{
    int i;

    for (i = 0; i < ad_len; i++) {
        buf[*buf_len++] = ad[i].data_len + 1;
        buf[*buf_len++] = ad[i].type;

        memcpy(&buf[*buf_len], ad[i].data,
               ad[i].data_len);
        *buf_len += ad[i].data_len;
    }

    return 0;
}

int
bt_le_adv_start(const struct bt_le_adv_param *param,
                const struct bt_data *ad, size_t ad_len,
                const struct bt_data *sd, size_t sd_len)
{
#if MYNEWT_VAL(BLE_EXT_ADV)
    uint8_t buf[MYNEWT_VAL(BLE_EXT_ADV_MAX_SIZE)];
#else
    uint8_t buf[BLE_HS_ADV_MAX_SZ];
#endif
    uint8_t buf_len;
    int err;

    err = set_ad(ad, ad_len, buf, &buf_len);
    if (err) {
        return err;
    }

    err = ble_gap_adv_set_data(buf, buf_len);
    if (err != 0) {
        return err;
    }

    err = ble_gap_adv_start(0x00, NULL, BLE_HS_FOREVER, param, NULL, NULL);
    if (err) {
        BT_ERR("Advertising failed: err %d", err);
        return err;
    }

    return 0;
}
