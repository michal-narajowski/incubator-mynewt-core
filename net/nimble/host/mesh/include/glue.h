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

#ifndef _MESH_GLUE_
#define _MESH_GLUE_

#include <assert.h>

#include "syscfg/syscfg.h"

#include "os/os_mbuf.h"
#include "os/os_mbuf.h"
#include "os/os_callout.h"
#include "os/os_eventq.h"

#include "atomic.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "../src/ble_sm_priv.h"
#include "../src/ble_hs_hci_priv.h"

#include "tinycrypt/aes.h"
#include "tinycrypt/constants.h"
#include "tinycrypt/utils.h"
#include "tinycrypt/cmac_mode.h"
#include "tinycrypt/ecc_dh.h"

#define u8_t    uint8_t
#define s8_t    int8_t
#define u16_t   uint16_t
#define u32_t   uint32_t
#define u64_t   uint64_t
#define s64_t   int64_t
#define s32_t   int32_t
typedef size_t ssize_t;
#define bt_conn ble_hs_conn

#define sys_put_be16(a,b) put_be16(b, a)
#define sys_put_le16(a,b) put_le16(b, a)
#define sys_put_be32(a,b) put_be32(b, a)
#define sys_get_be16 get_be16
#define sys_get_le16 get_le16
#define sys_get_be32 get_be32
#define sys_cpu_to_be16 htobe16
#define sys_cpu_to_be32 htobe32
#define sys_be32_to_cpu be32toh
#define sys_be16_to_cpu be16toh
#define sys_le16_to_cpu le16toh

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define CODE_UNREACHABLE __builtin_unreachable()
#define __ASSERT(code, str) do { \
		BT_ERR(str);\
		assert(code); \
    } while (0);

#define popcount(x) __builtin_popcount(x)
#define __packed    __attribute__((__packed__))

#define MSEC_PER_SEC   (1000)
#define K_MSEC(ms)     (ms)
#define K_SECONDS(s)   K_MSEC((s) * MSEC_PER_SEC)
#define K_MINUTES(m)   K_SECONDS((m) * 60)
#define K_HOURS(h)     K_MINUTES((h) * 60)

#define BT_GAP_ADV_FAST_INT_MIN_1               0x0030  /* 30 ms    */
#define BT_GAP_ADV_FAST_INT_MAX_1               0x0060  /* 60 ms    */
#define BT_GAP_ADV_FAST_INT_MIN_2               0x00a0  /* 100 ms   */
#define BT_GAP_ADV_FAST_INT_MAX_2               0x00f0  /* 150 ms   */
#define BT_GAP_ADV_SLOW_INT_MIN                 0x0640  /* 1 s      */
#define BT_GAP_ADV_SLOW_INT_MAX                 0x0780  /* 1.2 s    */

#define BT_WARN(...) BLE_HS_LOG(WARN, __VA_ARGS__ ); BLE_HS_LOG(WARN,"\n")
#define BT_DBG(...) BLE_HS_LOG(DEBUG, __VA_ARGS__); BLE_HS_LOG(DEBUG,"\n")
#define BT_INFO(...) BLE_HS_LOG(INFO, __VA_ARGS__); BLE_HS_LOG(INFO,"\n")
#define BT_ERR(...) BLE_HS_LOG(ERROR, __VA_ARGS__); BLE_HS_LOG(ERROR,"\n")

#define bt_addr_le_t ble_addr_t
#define bt_le_adv_param ble_gap_adv_params

#define net_buf_simple os_mbuf
#define net_buf os_mbuf
#define k_fifo os_eventq
#define k_fifo_init os_eventq_init
#define net_buf_simple_tailroom OS_MBUF_TRAILINGSPACE
#define net_buf_tailroom net_buf_simple_tailroom
#define net_buf_headroom OS_MBUF_LEADINGSPACE
static inline u8_t *net_buf_simple_tail(struct net_buf_simple *buf)
{
    return buf->om_data + buf->om_len;
}

struct net_buf_simple_state {
    /** Offset of the data pointer from the beginning of the storage */
    u16_t offset;
    /** Length of data */
    u16_t len;
};

#define NET_BUF_SIMPLE(a) os_msys_get(a, 0)
#define K_NO_WAIT 0
#define K_FOREVER (-1)

const char * bt_hex(const void *buf, size_t len);

void net_buf_put(struct k_fifo *fifo, struct net_buf *buf);
void * net_buf_ref(struct os_mbuf *om);
void net_buf_unref(struct os_mbuf *om);
uint16_t net_buf_simple_pull_le16(struct os_mbuf *om);
uint16_t net_buf_simple_pull_be16(struct os_mbuf *om);
uint32_t net_buf_simple_pull_be32(struct os_mbuf *om);
uint8_t net_buf_simple_pull_u8(struct os_mbuf *om);
void net_buf_simple_add_le16(struct os_mbuf *om, uint16_t val);
void net_buf_simple_add_be16(struct os_mbuf *om, uint16_t val);
void net_buf_simple_add_u8(struct os_mbuf *om, uint8_t val);
void net_buf_simple_add_be32(struct os_mbuf *om, uint32_t val);
void net_buf_add_zeros(struct os_mbuf *om, uint8_t len);
void net_buf_simple_push_le16(struct os_mbuf *om, uint16_t val);
void net_buf_simple_push_be16(struct os_mbuf *om, uint16_t val);
void net_buf_simple_push_u8(struct os_mbuf *om, uint8_t val);
void *net_buf_simple_pull(struct os_mbuf *om, uint8_t len);
void *net_buf_simple_add(struct os_mbuf *om, uint8_t len);
bool k_fifo_is_empty(struct os_eventq *q);
void * net_buf_get(struct k_fifo *fifo,s32_t t);
uint8_t *net_buf_simple_push(struct os_mbuf *om, uint8_t len);
void net_buf_reserve(struct net_buf *buf, size_t reserve);

#define net_buf_add_mem os_mbuf_append
#define net_buf_simple_add_mem os_mbuf_append
#define net_buf_add_u8 net_buf_simple_add_u8
#define net_buf_add net_buf_simple_add

/* This is by purpose */
#define net_buf_simple_init(msg, len)

#define net_buf_clone(buf, time) os_mbuf_dup(buf)
#define net_buf_add_be32 net_buf_simple_add_be32
#define net_buf_add_be16 net_buf_simple_add_be16

int bt_encrypt_le(const uint8_t *key, const uint8_t *plaintext, uint8_t *enc_data);
int bt_encrypt_be(const uint8_t *key, const uint8_t *plaintext, uint8_t *enc_data);
#define bt_le_adv_stop ble_gap_adv_stop
uint16_t bt_gatt_get_mtu(struct ble_hs_conn *conn);

struct bt_pub_key_cb {
    /** @brief Callback type for Public Key generation.
     *
     *  Used to notify of the local public key or that the local key is not
     *  available (either because of a failure to read it or because it is
     *  being regenerated).
     *
     *  @param key The local public key, or NULL in case of no key.
     */
    void (*func)(const u8_t key[64]);

    struct bt_pub_key_cb *_next;
};
typedef void (*bt_dh_key_cb_t)(const u8_t key[32]);
int bt_dh_key_gen(const u8_t remote_pk[64], bt_dh_key_cb_t cb);
int bt_pub_key_gen(struct bt_pub_key_cb *new_cb);
uint8_t *bt_pub_key_get(void);
int bt_rand(void *buf, size_t len);
#define bt_conn_ref(conn) conn;

struct k_delayed_work {
    struct os_callout work;
};

#define k_work os_callout
#define k_work_handler_t os_event_fn

void k_work_init(struct k_work *work, k_work_handler_t handler);
void k_delayed_work_init(struct k_delayed_work *w, os_event_fn *f);
void k_delayed_work_cancel(struct k_delayed_work *w);
void k_delayed_work_submit(struct k_delayed_work *w, uint32_t ms);
int64_t k_uptime_get(void);
u32_t k_uptime_get_32(void);
void k_work_submit(struct k_work *w);
uint32_t k_delayed_work_remaining_get (struct k_delayed_work *w);

static inline void net_buf_simple_save(struct net_buf_simple *buf,
                       struct net_buf_simple_state *state)
{
    //state->offset = net_buf_simple_headroom(buf);
    //state->len = buf->len;
}

static inline void net_buf_simple_restore(struct net_buf_simple *buf,
                                          struct net_buf_simple_state *state)
{
 //   buf->data = buf->__buf + state->offset;
 //   buf->len = state->len;
}

static inline void sys_memcpy_swap(void *dst, const void *src, size_t length)
{
    __ASSERT(((src < dst && (src + length) <= dst) ||
          (src > dst && (dst + length) <= src)),
         "Source and destination buffers must not overlap");

    src += length - 1;

    for (; length > 0; length--) {
        *((u8_t *)dst++) = *((u8_t *)src--);
    }
}

/* HOW TO HANDLE IT ??? */
static inline unsigned int find_lsb_set(u32_t op)
{
    return __builtin_ffs(op);
}

static inline unsigned int find_msb_set(u32_t op)
{
    if (!op)
        return 0;

    return 32 - __builtin_clz(op);
}


#endif
