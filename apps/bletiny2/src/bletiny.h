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

#ifndef H_BLETINY_PRIV_
#define H_BLETINY_PRIV_

#include <inttypes.h>
#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "log/log.h"
#include "os/queue.h"

#include "host/ble_gatt.h"
#ifdef __cplusplus
extern "C" {
#endif

struct ble_gap_white_entry;
struct ble_hs_adv_fields;
struct ble_gap_upd_params;
struct ble_gap_conn_params;
struct hci_adv_params;
struct ble_l2cap_sig_update_req;
struct ble_l2cap_sig_update_params;
union ble_store_value;
union ble_store_key;
struct ble_gap_adv_params;
struct ble_gap_conn_desc;
struct ble_gap_disc_params;

struct bletiny_dsc {
    SLIST_ENTRY(bletiny_dsc) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(bletiny_dsc_list, bletiny_dsc);

struct bletiny_chr {
    SLIST_ENTRY(bletiny_chr) next;
    struct ble_gatt_chr chr;

    struct bletiny_dsc_list dscs;
};
SLIST_HEAD(bletiny_chr_list, bletiny_chr);

struct bletiny_svc {
    SLIST_ENTRY(bletiny_svc) next;
    struct ble_gatt_svc svc;

    struct bletiny_chr_list chrs;
};

SLIST_HEAD(bletiny_svc_list, bletiny_svc);

struct bletiny_l2cap_coc {
    SLIST_ENTRY(bletiny_l2cap_coc) next;
    struct ble_l2cap_chan *chan;
};

SLIST_HEAD(bletiny_l2cap_coc_list, bletiny_l2cap_coc);

struct bletiny_conn {
    uint16_t handle;
    struct bletiny_svc_list svcs;
    struct bletiny_l2cap_coc_list coc_list;
};

extern struct bletiny_conn bletiny_conns[MYNEWT_VAL(BLE_MAX_CONNECTIONS)];
extern int bletiny_num_conns;

extern uint16_t nm_attr_val_handle;

extern struct log bletiny_log;
int nm_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                  uint8_t op, struct ble_gatt_access_ctxt *ctxt,
                  void *arg);
int nm_rx_rsp(uint8_t *attr_val, uint16_t attr_len);
void nm_init(void);
void bletiny_lock(void);
void bletiny_unlock(void);
int bletiny_exchange_mtu(uint16_t conn_handle);
int bletiny_disc_svcs(uint16_t conn_handle);
int bletiny_disc_svc_by_uuid(uint16_t conn_handle, const ble_uuid_t *uuid);
int bletiny_disc_all_chrs(uint16_t conn_handle, uint16_t start_handle,
                           uint16_t end_handle);
int bletiny_disc_chrs_by_uuid(uint16_t conn_handle, uint16_t start_handle,
                               uint16_t end_handle, const ble_uuid_t *uuid);
int bletiny_disc_all_dscs(uint16_t conn_handle, uint16_t start_handle,
                          uint16_t end_handle);
int bletiny_disc_full(uint16_t conn_handle);
int bletiny_find_inc_svcs(uint16_t conn_handle, uint16_t start_handle,
                           uint16_t end_handle);
int bletiny_read(uint16_t conn_handle, uint16_t attr_handle);
int bletiny_read_long(uint16_t conn_handle, uint16_t attr_handle,
                      uint16_t offset);
int bletiny_read_by_uuid(uint16_t conn_handle, uint16_t start_handle,
                          uint16_t end_handle, const ble_uuid_t *uuid);
int bletiny_read_mult(uint16_t conn_handle, uint16_t *attr_handles,
                       int num_attr_handles);
int bletiny_write(uint16_t conn_handle, uint16_t attr_handle,
                  struct os_mbuf *om);
int bletiny_write_no_rsp(uint16_t conn_handle, uint16_t attr_handle,
                         struct os_mbuf *om);
int bletiny_write_long(uint16_t conn_handle, uint16_t attr_handle,
                       uint16_t offset, struct os_mbuf *om);
int bletiny_write_reliable(uint16_t conn_handle,
                           struct ble_gatt_attr *attrs, int num_attrs);
int bletiny_adv_start(uint8_t own_addr_type, const ble_addr_t *direct_addr,
                      int32_t duration_ms,
                      const struct ble_gap_adv_params *params);
int bletiny_adv_stop(void);
int bletiny_conn_initiate(uint8_t own_addr_type, const ble_addr_t *peer_addr,
                          int32_t duration_ms,
                          struct ble_gap_conn_params *params);
int bletiny_conn_cancel(void);
int bletiny_term_conn(uint16_t conn_handle, uint8_t reason);
int bletiny_wl_set(ble_addr_t *addrs, int addrs_count);
int bletiny_scan(uint8_t own_addr_type, int32_t duration_ms,
                 const struct ble_gap_disc_params *disc_params);
int bletiny_scan_cancel(void);
int bletiny_set_adv_data(struct ble_hs_adv_fields *adv_fields);
int bletiny_update_conn(uint16_t conn_handle,
                         struct ble_gap_upd_params *params);
void bletiny_chrup(uint16_t attr_handle);
int bletiny_datalen(uint16_t conn_handle, uint16_t tx_octets,
                    uint16_t tx_time);
int bletiny_l2cap_update(uint16_t conn_handle,
                          struct ble_l2cap_sig_update_params *params);
int bletiny_sec_start(uint16_t conn_handle);
int bletiny_sec_pair(uint16_t conn_handle);
int bletiny_sec_restart(uint16_t conn_handle, uint8_t *ltk, uint16_t ediv,
                        uint64_t rand_val, int auth);
int bletiny_tx_start(uint16_t handle, uint16_t len, uint16_t rate,
                     uint16_t num);
int bletiny_rssi(uint16_t conn_handle, int8_t *out_rssi);
int bletiny_l2cap_create_srv(uint16_t psm);
int bletiny_l2cap_connect(uint16_t conn, uint16_t psm);
int bletiny_l2cap_disconnect(uint16_t conn, uint16_t idx);
#define BLETINY_LOG_MODULE  (LOG_MODULE_PERUSER + 0)
#define BLETINY_LOG(lvl, ...) \
    LOG_ ## lvl(&bletiny_log, BLETINY_LOG_MODULE, __VA_ARGS__)

/** GATT server. */
#define GATT_SVR_SVC_ALERT_UUID               0x1811
#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID   0x2A47
#define GATT_SVR_CHR_NEW_ALERT                0x2A46
#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID   0x2A48
#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID      0x2A45
#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT        0x2A44

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int gatt_svr_init(void);

/** Misc. */
void print_bytes(const uint8_t *bytes, int len);
void print_mbuf(const struct os_mbuf *om);
void print_addr(const void *addr);
void print_uuid(const ble_uuid_t *uuid);
int svc_is_empty(const struct bletiny_svc *svc);
uint16_t chr_end_handle(const struct bletiny_svc *svc,
                        const struct bletiny_chr *chr);
int chr_is_empty(const struct bletiny_svc *svc, const struct bletiny_chr *chr);
void print_conn_desc(const struct ble_gap_conn_desc *desc);

#ifdef __cplusplus
}
#endif

#endif