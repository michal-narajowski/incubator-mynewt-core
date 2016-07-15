/**
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

#ifndef H_BLECENT_
#define H_BLECENT_

#include "os/queue.h"
#include "log/log.h"
struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

extern struct log blecent_log;

/* blecent uses the first "peruser" log module. */
#define BLECENT_LOG_MODULE  (LOG_MODULE_PERUSER + 0)

/* Convenience macro for logging to the blecent module. */
#define BLECENT_LOG(lvl, ...) \
    LOG_ ## lvl(&blecent_log, BLECENT_LOG_MODULE, __VA_ARGS__)

#define BLECENT_SVC_ALERT_UUID              0x1811
#define BLECENT_CHR_SUP_NEW_ALERT_CAT_UUID  0x2A47
#define BLECENT_CHR_NEW_ALERT               0x2A46
#define BLECENT_CHR_SUP_UNR_ALERT_CAT_UUID  0x2A48
#define BLECENT_CHR_UNR_ALERT_STAT_UUID     0x2A45
#define BLECENT_CHR_ALERT_NOT_CTRL_PT       0x2A44

/** GATT server. */
void gatt_svr_register(void);
void gatt_svr_init_cfg(struct ble_hs_cfg *cfg);


/** Misc. */
void print_bytes(const uint8_t *bytes, int len);
char *addr_str(const void *addr);
void print_uuid(const void *uuid128);
void print_conn_desc(const struct ble_gap_conn_desc *desc);
void print_adv_fields(const struct ble_hs_adv_fields *fields);

/** Peer. */
struct peer_dsc {
    SLIST_ENTRY(peer_dsc) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(peer_dsc_list, peer_dsc);

struct peer_chr {
    SLIST_ENTRY(peer_chr) next;
    struct ble_gatt_chr chr;

    struct peer_dsc_list dscs;
};
SLIST_HEAD(peer_chr_list, peer_chr);

struct peer_svc {
    SLIST_ENTRY(peer_svc) next;
    struct ble_gatt_svc svc;

    struct peer_chr_list chrs;
};
SLIST_HEAD(peer_svc_list, peer_svc);

struct peer;
typedef void peer_disc_fn(const struct peer *peer, int status, void *arg);

struct peer {
    SLIST_ENTRY(peer) next;

    uint16_t conn_handle;

    /** List of discovered GATT services. */
    struct peer_svc_list svcs;

    /** Keeps track of where we are in the service discovery process. */
    uint16_t disc_prev_chr_val;
    struct peer_svc *cur_svc;

    /** Callback that gets executed when service discovery completes. */
    peer_disc_fn *disc_cb;
    void *disc_cb_arg;
};

int peer_disc_all(uint16_t conn_handle, peer_disc_fn *disc_cb,
                  void *disc_cb_arg);
const struct peer_dsc *
peer_dsc_find_uuid(const struct peer *peer, const uint8_t *svc_uuid128,
                   const uint8_t *chr_uuid128, const uint8_t *dsc_uuid128);
const struct peer_chr *
peer_chr_find_uuid(const struct peer *peer, const uint8_t *svc_uuid128,
                   const uint8_t *chr_uuid128);
const struct peer_svc *
peer_svc_find_uuid(const struct peer *peer, const uint8_t *uuid128);
int peer_delete(uint16_t conn_handle);
int peer_add(uint16_t conn_handle);
int peer_init(int max_peers, int max_svcs, int max_chrs, int max_dscs);

#endif
