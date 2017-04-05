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

#include "syscfg/syscfg.h"

#include <assert.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include "bsp/bsp.h"

#include "nimble/ble.h"
#include "nimble/nimble_opt.h"
#include "nimble/hci_common.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "host/ble_eddystone.h"
#include "host/ble_hs_id.h"
#include "services/gatt/ble_svc_gatt.h"
#include "../src/ble_hs_priv.h"

#include "console/console.h"
#include "shell/shell.h"

#include "cmd.h"
#include "bletiny.h"
#include "cmd_gatt.h"
#include "cmd_l2cap.h"

#define BTSHELL_MODULE "btshell"


int
cmd_parse_conn_start_end(uint16_t *out_conn, uint16_t *out_start,
                         uint16_t *out_end)
{
    int rc;

    *out_conn = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        return rc;
    }

    *out_start = parse_arg_uint16("start", &rc);
    if (rc != 0) {
        return rc;
    }

    *out_end = parse_arg_uint16("end", &rc);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

static struct kv_pair cmd_own_addr_types[] = {
    { "public",     BLE_OWN_ADDR_PUBLIC },
    { "random",     BLE_OWN_ADDR_RANDOM },
    { "rpa_pub",    BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT },
    { "rpa_rnd",    BLE_OWN_ADDR_RPA_RANDOM_DEFAULT },
    { NULL }
};

static struct kv_pair cmd_peer_addr_types[] = {
    { "public",     BLE_ADDR_PUBLIC },
    { "random",     BLE_ADDR_RANDOM },
    { "public_id",  BLE_ADDR_PUBLIC_ID },
    { "random_id",  BLE_ADDR_RANDOM_ID },
    { NULL }
};

static struct kv_pair cmd_addr_type[] = {
    { "public",     BLE_ADDR_PUBLIC },
    { "random",     BLE_ADDR_RANDOM },
    { NULL }
};



/*****************************************************************************
 * $advertise                                                                *
 *****************************************************************************/

static struct kv_pair cmd_adv_conn_modes[] = {
    { "non", BLE_GAP_CONN_MODE_NON },
    { "und", BLE_GAP_CONN_MODE_UND },
    { "dir", BLE_GAP_CONN_MODE_DIR },
    { NULL }
};

static struct kv_pair cmd_adv_disc_modes[] = {
    { "non", BLE_GAP_DISC_MODE_NON },
    { "ltd", BLE_GAP_DISC_MODE_LTD },
    { "gen", BLE_GAP_DISC_MODE_GEN },
    { NULL }
};

static struct kv_pair cmd_adv_filt_types[] = {
    { "none", BLE_HCI_ADV_FILT_NONE },
    { "scan", BLE_HCI_ADV_FILT_SCAN },
    { "conn", BLE_HCI_ADV_FILT_CONN },
    { "both", BLE_HCI_ADV_FILT_BOTH },
    { NULL }
};

static int
cmd_advertise(int argc, char **argv)
{
    struct ble_gap_adv_params params;
    int32_t duration_ms;
    ble_addr_t peer_addr;
    ble_addr_t *peer_addr_param = &peer_addr;
    uint8_t own_addr_type;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    if (argc > 1 && strcmp(argv[1], "stop") == 0) {
        rc = bletiny_adv_stop();
        if (rc != 0) {
            console_printf("advertise stop fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    params.conn_mode = parse_arg_kv_default("conn", cmd_adv_conn_modes,
                                            BLE_GAP_CONN_MODE_UND, &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    params.disc_mode = parse_arg_kv_default("discov", cmd_adv_disc_modes,
                                            BLE_GAP_DISC_MODE_GEN, &rc);
    if (rc != 0) {
        console_printf("invalid 'discov' parameter\n");
        return rc;
    }

    peer_addr.type = parse_arg_kv_default(
        "peer_addr_type", cmd_peer_addr_types, BLE_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'peer_addr_type' parameter\n");
        return rc;
    }

    rc = parse_arg_mac("peer_addr", peer_addr.val);
    if (rc == ENOENT) {
        peer_addr_param = NULL;
    } else if (rc != 0) {
        console_printf("invalid 'peer_addr' parameter\n");
        return rc;
    }

    own_addr_type = parse_arg_kv_default(
        "own_addr_type", cmd_own_addr_types, BLE_OWN_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'own_addr_type' parameter\n");
        return rc;
    }

    params.channel_map = parse_arg_long_bounds_default("channel_map", 0, 0xff, 0,
                                                       &rc);
    if (rc != 0) {
        console_printf("invalid 'channel_map' parameter\n");
        return rc;
    }

    params.filter_policy = parse_arg_kv_default("filter", cmd_adv_filt_types,
                                                BLE_HCI_ADV_FILT_NONE, &rc);
    if (rc != 0) {
        console_printf("invalid 'filter' parameter\n");
        return rc;
    }

    params.itvl_min = parse_arg_long_bounds_default("interval_min", 0, UINT16_MAX,
                                                    0, &rc);
    if (rc != 0) {
        console_printf("invalid 'interval_min' parameter\n");
        return rc;
    }

    params.itvl_max = parse_arg_long_bounds_default("interval_max", 0, UINT16_MAX,
                                                    0, &rc);
    if (rc != 0) {
        console_printf("invalid 'interval_max' parameter\n");
        return rc;
    }

    params.high_duty_cycle = parse_arg_long_bounds_default("high_duty", 0, 1, 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'high_duty' parameter\n");
        return rc;
    }

    duration_ms = parse_arg_long_bounds_default("duration", 1, INT32_MAX,
                                                BLE_HS_FOREVER, &rc);
    if (rc != 0) {
        console_printf("invalid 'duration' parameter\n");
        return rc;
    }

    rc = bletiny_adv_start(own_addr_type, peer_addr_param, duration_ms,
                           &params);
    if (rc != 0) {
        console_printf("advertise fail: %d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param advertise_params[] = {
    {"stop", "stop advertising procedure"},
    {"conn", "connectable mode, usage: =[non|und|dir], default: und"},
    {"discov", "discoverable mode, usage: =[non|ltd|gen], default: gen"},
    {"peer_addr_type", "usage: =[public|random|public_id|random_id], default: public"},
    {"peer_addr", "usage: =[XX:XX:XX:XX:XX:XX]"},
    {"own_addr_type", "usage: =[public|random|rpa_pub|rpa_rnd], default: public"},
    {"channel_map", "usage: =[0x00-0xff], default: 0"},
    {"filter", "usage: =[none|scan|conn|both], default: none"},
    {"interval_min", "usage: =[0-UINT16_MAX], default: 0"},
    {"interval_max", "usage: =[0-UINT16_MAX], default: 0"},
    {"high_duty", "usage: =[0-1], default: 0"},
    {"duration", "usage: =[1-INT32_MAX], default: INT32_MAX"},
    {NULL, NULL}
};

static const struct shell_cmd_help advertise_help = {
    .summary = "advertise",
    .usage = "advertise usage",
    .params = advertise_params,
};

/*****************************************************************************
 * $connect                                                                  *
 *****************************************************************************/

static int
cmd_connect(int argc, char **argv)
{
    struct ble_gap_conn_params params;
    int32_t duration_ms;
    ble_addr_t peer_addr;
    ble_addr_t *peer_addr_param = &peer_addr;
    int own_addr_type;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    if (argc > 1 && strcmp(argv[1], "cancel") == 0) {
        rc = bletiny_conn_cancel();
        if (rc != 0) {
            console_printf("connection cancel fail: %d\n", rc);
            return rc;
        }

        return 0;
    }

    peer_addr.type = parse_arg_kv_default("peer_addr_type", cmd_peer_addr_types,
                                          BLE_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'peer_addr_type' parameter\n");
        return rc;
    }

    rc = parse_arg_mac("peer_addr", peer_addr.val);
    if (rc == ENOENT) {
        /* Allow "addr" for backwards compatibility. */
        rc = parse_arg_mac("addr", peer_addr.val);
    }

    if (rc == ENOENT) {
        /* With no "peer_addr" specified we'll use white list */
        peer_addr_param = NULL;
    } else if (rc != 0) {
        console_printf("invalid 'peer_addr' parameter\n");
        return rc;
    }

    own_addr_type = parse_arg_kv_default("own_addr_type", cmd_own_addr_types,
                                         BLE_OWN_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'own_addr_type' parameter\n");
        return rc;
    }

    params.scan_itvl = parse_arg_uint16_dflt("scan_interval", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'scan_interval' parameter\n");
        return rc;
    }

    params.scan_window = parse_arg_uint16_dflt("scan_window", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'scan_window' parameter\n");
        return rc;
    }

    params.itvl_min = parse_arg_uint16_dflt(
        "interval_min", BLE_GAP_INITIAL_CONN_ITVL_MIN, &rc);
    if (rc != 0) {
        console_printf("invalid 'interval_min' parameter\n");
        return rc;
    }

    params.itvl_max = parse_arg_uint16_dflt(
        "interval_max", BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        console_printf("invalid 'interval_max' parameter\n");
        return rc;
    }

    params.latency = parse_arg_uint16_dflt("latency", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'latency' parameter\n");
        return rc;
    }

    params.supervision_timeout = parse_arg_uint16_dflt("timeout", 0x0100, &rc);
    if (rc != 0) {
        console_printf("invalid 'timeout' parameter\n");
        return rc;
    }

    params.min_ce_len = parse_arg_uint16_dflt("min_conn_event_len", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'min_conn_event_len' parameter\n");
        return rc;
    }

    params.max_ce_len = parse_arg_uint16_dflt("max_conn_event_len", 0x0300, &rc);
    if (rc != 0) {
        console_printf("invalid 'max_conn_event_len' parameter\n");
        return rc;
    }

    duration_ms = parse_arg_long_bounds_default("duration", 1, INT32_MAX, 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'duration' parameter\n");
        return rc;
    }

    rc = bletiny_conn_initiate(own_addr_type, peer_addr_param, duration_ms,
                               &params);
    if (rc != 0) {
        console_printf("error connecting; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param connect_params[] = {
    {"cancel", "cancel connection procedure"},
    {"peer_addr_type", "usage: =[public|random|public_id|random_id], default: public"},
    {"peer_addr", "usage: =[XX:XX:XX:XX:XX:XX]"},
    {"own_addr_type", "usage: =[public|random|rpa_pub|rpa_rnd], default: public"},
    {"scan_interval", "usage: =[0-UINT16_MAX], default: 0x0010"},
    {"scan_window", "usage: =[0-UINT16_MAX], default: 0x0010"},
    {"interval_min", "usage: =[0-UINT16_MAX], default: 30"},
    {"interval_max", "usage: =[0-UINT16_MAX], default: 50"},
    {"latency", "usage: =[UINT16], default: 0"},
    {"timeout", "usage: =[UINT16], default: 0x0100"},
    {"min_conn_event_len", "usage: =[UINT16], default: 0x0010"},
    {"max_conn_event_len", "usage: =[UINT16], default: 0x0300"},
    {"duration", "usage: =[1-INT32_MAX], default: 0"},
    {NULL, NULL}
};

static const struct shell_cmd_help connect_help = {
    .summary = "connect",
    .usage = "connect usage",
    .params = connect_params,
};


/*****************************************************************************
 * $disconnect                                                               *
 *****************************************************************************/

static int
cmd_disconnect(int argc, char **argv)
{
    uint16_t conn_handle;
    uint8_t reason;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    reason = parse_arg_uint8_dflt("reason", BLE_ERR_REM_USER_CONN_TERM, &rc);
    if (rc != 0) {
        console_printf("invalid 'reason' parameter\n");
        return rc;
    }

    rc = bletiny_term_conn(conn_handle, reason);
    if (rc != 0) {
        console_printf("error terminating connection; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param disconnect_params[] = {
    {"conn", "connection handle parameter, usage: =<UINT16>"},
    {"reason", "disconnection reason, usage: =[UINT8], default: 19 (remote user terminated connection)"},
    {NULL, NULL}
};

static const struct shell_cmd_help disconnect_help = {
    .summary = "disconnect",
    .usage = "disconnect usage",
    .params = disconnect_params,
};

/*****************************************************************************
 * $scan                                                                     *
 *****************************************************************************/

static struct kv_pair cmd_scan_filt_policies[] = {
    { "no_wl", BLE_HCI_SCAN_FILT_NO_WL },
    { "use_wl", BLE_HCI_SCAN_FILT_USE_WL },
    { "no_wl_inita", BLE_HCI_SCAN_FILT_NO_WL_INITA },
    { "use_wl_inita", BLE_HCI_SCAN_FILT_USE_WL_INITA },
    { NULL }
};

static int
cmd_scan(int argc, char **argv)
{
    struct ble_gap_disc_params params;
    int32_t duration_ms;
    uint8_t own_addr_type;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    if (argc > 1 && strcmp(argv[1], "cancel") == 0) {
        rc = bletiny_scan_cancel();
        if (rc != 0) {
            console_printf("connection cancel fail: %d\n", rc);
            return rc;
        }
        return 0;
    }

    duration_ms = parse_arg_long_bounds_default("duration", 1, INT32_MAX,
                                                BLE_HS_FOREVER, &rc);
    if (rc != 0) {
        console_printf("invalid 'duration' parameter\n");
        return rc;
    }

    params.limited = parse_arg_bool_default("limited", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'limited' parameter\n");
        return rc;
    }

    params.passive = parse_arg_bool_default("passive", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'passive' parameter\n");
        return rc;
    }

    params.itvl = parse_arg_uint16_dflt("interval", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'interval' parameter\n");
        return rc;
    }

    params.window = parse_arg_uint16_dflt("window", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'window' parameter\n");
        return rc;
    }

    params.filter_policy = parse_arg_kv_default(
        "filter", cmd_scan_filt_policies, BLE_HCI_SCAN_FILT_NO_WL, &rc);
    if (rc != 0) {
        console_printf("invalid 'filter' parameter\n");
        return rc;
    }

    params.filter_duplicates = parse_arg_bool_default("nodups", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'nodups' parameter\n");
        return rc;
    }

    own_addr_type = parse_arg_kv_default("own_addr_type", cmd_own_addr_types,
                                         BLE_OWN_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'own_addr_type' parameter\n");
        return rc;
    }

    rc = bletiny_scan(own_addr_type, duration_ms, &params);
    if (rc != 0) {
        console_printf("error scanning; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param scan_params[] = {
    {"cancel", "cancel scan procedure"},
    {"duration", "usage: =[1-INT32_MAX], default: INT32_MAX"},
    {"limited", "usage: =[0-1], default: 0"},
    {"passive", "usage: =[0-1], default: 0"},
    {"interval", "usage: =[0-UINT16_MAX], default: 0"},
    {"window", "usage: =[0-UINT16_MAX], default: 0"},
    {"filter", "usage: =[no_wl|use_wl|no_wl_inita|use_wl_inita], default: no_wl"},
    {"nodups", "usage: =[0-UINT16_MAX], default: 0"},
    {"own_addr_type", "usage: =[public|random|rpa_pub|rpa_rnd], default: public"},
    {NULL, NULL}
};

static const struct shell_cmd_help scan_help = {
    .summary = "scan",
    .usage = "scan usage",
    .params = scan_params,
};

/*****************************************************************************
 * $set                                                                      *
 *****************************************************************************/

static struct kv_pair cmd_set_addr_types[] = {
    { "public",         BLE_ADDR_PUBLIC },
    { "random",         BLE_ADDR_RANDOM },
    { NULL }
};

static int
cmd_set_addr(void)
{
    uint8_t addr[6];
    int addr_type;
    int rc;

    addr_type = parse_arg_kv_default("addr_type", cmd_set_addr_types,
                                     BLE_ADDR_PUBLIC, &rc);
    if (rc != 0) {
        console_printf("invalid 'addr_type' parameter\n");
        return rc;
    }

    rc = parse_arg_mac("addr", addr);
    if (rc != 0) {
        console_printf("invalid 'addr' parameter\n");
        return rc;
    }

    switch (addr_type) {
    case BLE_ADDR_PUBLIC:
        /* We shouldn't be writing to the controller's address (g_dev_addr).
         * There is no standard way to set the local public address, so this is
         * our only option at the moment.
         */
        memcpy(g_dev_addr, addr, 6);
        ble_hs_id_set_pub(g_dev_addr);
        break;

    case BLE_ADDR_RANDOM:
        rc = ble_hs_id_set_rnd(addr);
        if (rc != 0) {
            return rc;
        }
        break;

    default:
        return BLE_HS_EUNKNOWN;
    }

    return 0;
}

static int
cmd_set(int argc, char **argv)
{
    uint16_t mtu;
    uint8_t irk[16];
    int good = 0;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    rc = parse_arg_find_idx("addr");
    if (rc != -1) {
        rc = cmd_set_addr();
        if (rc != 0) {
            return rc;
        }
        good = 1;
    }

    mtu = parse_arg_uint16("mtu", &rc);
    if (rc == 0) {
        rc = ble_att_set_preferred_mtu(mtu);
        if (rc == 0) {
            good = 1;
        }
    } else if (rc != ENOENT) {
        console_printf("invalid 'mtu' parameter\n");
        return rc;
    }

    rc = parse_arg_byte_stream_exact_length("irk", irk, 16);
    if (rc == 0) {
        good = 1;
        ble_hs_pvcy_set_our_irk(irk);
    } else if (rc != ENOENT) {
        console_printf("invalid 'irk' parameter\n");
        return rc;
    }

    if (!good) {
        console_printf("Error: no valid settings specified\n");
        return -1;
    }

    return 0;
}

static const struct shell_param set_params[] = {
    {"addr", "set device address, usage: =[XX:XX:XX:XX:XX:XX]"},
    {"addr_type", "set device address type, usage: =[public|random|public_id|random_id], default: public"},
    {"mtu", "Maximum Transimssion Unit, usage: =[0-UINT16_MAX]"},
    {"irk", "Identity Resolving Key, usage: =[XX:XX...], len=16 octets"},
    {NULL, NULL}
};

static const struct shell_cmd_help set_help = {
    .summary = "set",
    .usage = "set usage",
    .params = set_params,
};

/*****************************************************************************
 * $set-adv-data                                                             *
 *****************************************************************************/

#define CMD_ADV_DATA_MAX_UUIDS16                8
#define CMD_ADV_DATA_MAX_UUIDS32                8
#define CMD_ADV_DATA_MAX_UUIDS128               2
#define CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS       8
#define CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN    BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN    BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN   BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_URI_MAX_LEN                BLE_HS_ADV_MAX_FIELD_SZ
#define CMD_ADV_DATA_MFG_DATA_MAX_LEN           BLE_HS_ADV_MAX_FIELD_SZ

static int
cmd_set_adv_data(int argc, char **argv)
{
    static bssnz_t ble_uuid16_t uuids16[CMD_ADV_DATA_MAX_UUIDS16];
    static bssnz_t ble_uuid32_t uuids32[CMD_ADV_DATA_MAX_UUIDS32];
    static bssnz_t ble_uuid128_t uuids128[CMD_ADV_DATA_MAX_UUIDS128];
    static bssnz_t uint8_t
        public_tgt_addrs[CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS]
                        [BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN];
    static bssnz_t uint8_t slave_itvl_range[BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN];
    static bssnz_t uint8_t
        svc_data_uuid16[CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN];
    static bssnz_t uint8_t
        svc_data_uuid32[CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN];
    static bssnz_t uint8_t
        svc_data_uuid128[CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN];
    static bssnz_t uint8_t uri[CMD_ADV_DATA_URI_MAX_LEN];
    static bssnz_t uint8_t mfg_data[CMD_ADV_DATA_MFG_DATA_MAX_LEN];
    struct ble_hs_adv_fields adv_fields;
    uint32_t uuid32;
    uint16_t uuid16;
    uint8_t uuid128[16];
    uint8_t public_tgt_addr[BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN];
    uint8_t eddystone_url_body_len;
    uint8_t eddystone_url_suffix;
    uint8_t eddystone_url_scheme;
    char eddystone_url_body[BLE_EDDYSTONE_URL_MAX_LEN];
    char *eddystone_url_full;
    int svc_data_uuid16_len;
    int svc_data_uuid32_len;
    int svc_data_uuid128_len;
    int uri_len;
    int mfg_data_len;
    int tmp;
    int rc;

    memset(&adv_fields, 0, sizeof adv_fields);

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    tmp = parse_arg_long_bounds("flags", 0, UINT8_MAX, &rc);
    if (rc == 0) {
        adv_fields.flags = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'flags' parameter\n");
        return rc;
    }

    while (1) {
        uuid16 = parse_arg_uint16("uuid16", &rc);
        if (rc == 0) {
            if (adv_fields.num_uuids16 >= CMD_ADV_DATA_MAX_UUIDS16) {
                console_printf("invalid 'uuid16' parameter\n");
                return EINVAL;
            }
            uuids16[adv_fields.num_uuids16] = (ble_uuid16_t) BLE_UUID16_INIT(uuid16);
            adv_fields.num_uuids16++;
        } else if (rc == ENOENT) {
            break;
        } else {
            console_printf("invalid 'uuid16' parameter\n");
            return rc;
        }
    }
    if (adv_fields.num_uuids16 > 0) {
        adv_fields.uuids16 = uuids16;
    }

    tmp = parse_arg_long("uuids16_is_complete", &rc);
    if (rc == 0) {
        adv_fields.uuids16_is_complete = !!tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'uuids16_is_complete' parameter\n");
        return rc;
    }

    while (1) {
        uuid32 = parse_arg_uint32("uuid32", &rc);
        if (rc == 0) {
            if (adv_fields.num_uuids32 >= CMD_ADV_DATA_MAX_UUIDS32) {
                console_printf("invalid 'uuid32' parameter\n");
                return EINVAL;
            }
            uuids32[adv_fields.num_uuids32] = (ble_uuid32_t) BLE_UUID32_INIT(uuid32);
            adv_fields.num_uuids32++;
        } else if (rc == ENOENT) {
            break;
        } else {
            console_printf("invalid 'uuid32' parameter\n");
            return rc;
        }
    }
    if (adv_fields.num_uuids32 > 0) {
        adv_fields.uuids32 = uuids32;
    }

    tmp = parse_arg_long("uuids32_is_complete", &rc);
    if (rc == 0) {
        adv_fields.uuids32_is_complete = !!tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'uuids32_is_complete' parameter\n");
        return rc;
    }

    while (1) {
        rc = parse_arg_byte_stream_exact_length("uuid128", uuid128, 16);
        if (rc == 0) {
            if (adv_fields.num_uuids128 >= CMD_ADV_DATA_MAX_UUIDS128) {
                console_printf("invalid 'uuid128' parameter\n");
                return EINVAL;
            }
            ble_uuid_init_from_buf((ble_uuid_any_t *) &uuids128[adv_fields.num_uuids128],
                                   uuid128, 16);
            adv_fields.num_uuids128++;
        } else if (rc == ENOENT) {
            break;
        } else {
            console_printf("invalid 'uuid128' parameter\n");
            return rc;
        }
    }
    if (adv_fields.num_uuids128 > 0) {
        adv_fields.uuids128 = uuids128;
    }

    tmp = parse_arg_long("uuids128_is_complete", &rc);
    if (rc == 0) {
        adv_fields.uuids128_is_complete = !!tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'uuids128_is_complete' parameter\n");
        return rc;
    }

    adv_fields.name = (uint8_t *)parse_arg_extract("name");
    if (adv_fields.name != NULL) {
        adv_fields.name_len = strlen((char *)adv_fields.name);
    }

    tmp = parse_arg_long_bounds("tx_power_level", INT8_MIN, INT8_MAX, &rc);
    if (rc == 0) {
        adv_fields.tx_pwr_lvl = tmp;
        adv_fields.tx_pwr_lvl_is_present = 1;
    } else if (rc != ENOENT) {
        console_printf("invalid 'tx_power_level' parameter\n");
        return rc;
    }

    rc = parse_arg_byte_stream_exact_length("slave_interval_range",
                                            slave_itvl_range,
                                            BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
    if (rc == 0) {
        adv_fields.slave_itvl_range = slave_itvl_range;
    } else if (rc != ENOENT) {
        console_printf("invalid 'slave_interval_range' parameter\n");
        return rc;
    }

    rc = parse_arg_byte_stream("service_data_uuid16",
                               CMD_ADV_DATA_SVC_DATA_UUID16_MAX_LEN,
                               svc_data_uuid16, &svc_data_uuid16_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid16 = svc_data_uuid16;
        adv_fields.svc_data_uuid16_len = svc_data_uuid16_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'service_data_uuid16' parameter\n");
        return rc;
    }

    while (1) {
        rc = parse_arg_byte_stream_exact_length(
            "public_target_address", public_tgt_addr,
            BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
        if (rc == 0) {
            if (adv_fields.num_public_tgt_addrs >=
                CMD_ADV_DATA_MAX_PUBLIC_TGT_ADDRS) {

                console_printf("invalid 'public_target_address' parameter\n");
                return EINVAL;
            }
            memcpy(public_tgt_addrs[adv_fields.num_public_tgt_addrs],
                   public_tgt_addr, BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN);
            adv_fields.num_public_tgt_addrs++;
        } else if (rc == ENOENT) {
            break;
        } else {
            console_printf("invalid 'public_target_address' parameter\n");
            return rc;
        }
    }
    if (adv_fields.num_public_tgt_addrs > 0) {
        adv_fields.public_tgt_addr = (void *)public_tgt_addrs;
    }

    adv_fields.appearance = parse_arg_uint16("appearance", &rc);
    if (rc == 0) {
        adv_fields.appearance_is_present = 1;
    } else if (rc != ENOENT) {
        console_printf("invalid 'appearance' parameter\n");
        return rc;
    }

    adv_fields.adv_itvl = parse_arg_uint16("advertising_interval", &rc);
    if (rc == 0) {
        adv_fields.adv_itvl_is_present = 1;
    } else if (rc != ENOENT) {
        console_printf("invalid 'advertising_interval' parameter\n");
        return rc;
    }

    rc = parse_arg_byte_stream("service_data_uuid32",
                               CMD_ADV_DATA_SVC_DATA_UUID32_MAX_LEN,
                               svc_data_uuid32, &svc_data_uuid32_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid32 = svc_data_uuid32;
        adv_fields.svc_data_uuid32_len = svc_data_uuid32_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'service_data_uuid32' parameter\n");
        return rc;
    }

    rc = parse_arg_byte_stream("service_data_uuid128",
                               CMD_ADV_DATA_SVC_DATA_UUID128_MAX_LEN,
                               svc_data_uuid128, &svc_data_uuid128_len);
    if (rc == 0) {
        adv_fields.svc_data_uuid128 = svc_data_uuid128;
        adv_fields.svc_data_uuid128_len = svc_data_uuid128_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'service_data_uuid128' parameter\n");
        return rc;
    }

    rc = parse_arg_byte_stream("uri", CMD_ADV_DATA_URI_MAX_LEN, uri, &uri_len);
    if (rc == 0) {
        adv_fields.uri = uri;
        adv_fields.uri_len = uri_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'uri' parameter\n");
        return rc;
    }

    rc = parse_arg_byte_stream("mfg_data", CMD_ADV_DATA_MFG_DATA_MAX_LEN,
                               mfg_data, &mfg_data_len);
    if (rc == 0) {
        adv_fields.mfg_data = mfg_data;
        adv_fields.mfg_data_len = mfg_data_len;
    } else if (rc != ENOENT) {
        console_printf("invalid 'mfg_data' parameter\n");
        return rc;
    }

    eddystone_url_full = parse_arg_extract("eddystone_url");
    if (eddystone_url_full != NULL) {
        rc = parse_eddystone_url(eddystone_url_full, &eddystone_url_scheme,
                                 eddystone_url_body,
                                 &eddystone_url_body_len,
                                 &eddystone_url_suffix);
        if (rc != 0) {
            return rc;
        }

        rc = ble_eddystone_set_adv_data_url(&adv_fields, eddystone_url_scheme,
                                            eddystone_url_body,
                                            eddystone_url_body_len,
                                            eddystone_url_suffix);
    } else {
        rc = bletiny_set_adv_data(&adv_fields);
    }
    if (rc != 0) {
        console_printf("error setting advertisement data; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param set_adv_data_params[] = {
    {"flags", "usage: =[0-UINT8_MAX]"},
    {"uuid16", "usage: =[UINT16]"},
    {"uuid16_is_complete", "usage: =[0-1]"},
    {"uuid32", "usage: =[UINT32]"},
    {"uuid32_is_complete", "usage: =[0-1]"},
    {"uuid128", "usage: =[XX:XX...], len=16 octets"},
    {"uuid128_is_complete", "usage: =[0-1]"},
    {"tx_power_level", "usage: =[INT8_MIN-INT8_MAX]"},
    {"slave_interval_range", "usage: =[XX:XX:XX:XX]"},
    {"public_target_address", "usage: =[XX:XX:XX:XX:XX:XX]"},
    {"appearance", "usage: =[UINT16]"},
    {"name", "usage: =[string]"},
    {"advertising_interval", "usage: =[UINT16]"},
    {"service_data_uuid16", "usage: =[XX:XX...]"},
    {"service_data_uuid32", "usage: =[XX:XX...]"},
    {"service_data_uuid128", "usage: =[XX:XX...]"},
    {"uri", "usage: =[XX:XX...]"},
    {"mfg_data", "usage: =[XX:XX...]"},
    {"eddystone_url", "usage: =[string]"},
    {NULL, NULL}
};

static const struct shell_cmd_help set_adv_data_help = {
    .summary = "set_adv_data",
    .usage = "set_adv_data usage",
    .params = set_adv_data_params,
};

/*****************************************************************************
 * $white-list                                                               *
 *****************************************************************************/

#define CMD_WL_MAX_SZ   8

static int
cmd_white_list(int argc, char **argv)
{
    static ble_addr_t addrs[CMD_WL_MAX_SZ];
    int addrs_cnt;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    addrs_cnt = 0;
    while (1) {
        if (addrs_cnt >= CMD_WL_MAX_SZ) {
            return EINVAL;
        }

        rc = parse_arg_mac("addr", addrs[addrs_cnt].val);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            console_printf("invalid 'addr' parameter\n");
            return rc;
        }

        addrs[addrs_cnt].type = parse_arg_kv("addr_type", cmd_addr_type, &rc);
        if (rc != 0) {
            console_printf("invalid 'addr' parameter\n");
            return rc;
        }

        addrs_cnt++;
    }

    if (addrs_cnt == 0) {
        return EINVAL;
    }

    bletiny_wl_set(addrs, addrs_cnt);

    return 0;
}

static const struct shell_param white_list_params[] = {
    {"addr", "white_list device address, usage: =[XX:XX:XX:XX:XX:XX]"},
    {"addr_type", "white_list device address type, usage: =[public|random]"},
    {NULL, NULL}
};

static const struct shell_cmd_help white_list_help = {
    .summary = "white_list",
    .usage = "white_list usage",
    .params = white_list_params,
};

/*****************************************************************************
 * $conn-rssi                                                                *
 *****************************************************************************/

static int
cmd_conn_rssi(int argc, char **argv)
{
    uint16_t conn_handle;
    int8_t rssi;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    rc = bletiny_rssi(conn_handle, &rssi);
    if (rc != 0) {
        console_printf("error reading rssi; rc=%d\n", rc);
        return rc;
    }

    console_printf("conn=%d rssi=%d\n", conn_handle, rssi);

    return 0;
}

static const struct shell_param conn_rssi_params[] = {
    {"conn", "connection handle parameter, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help conn_rssi_help = {
    .summary = "conn_rssi",
    .usage = "conn_rssi usage",
    .params = conn_rssi_params,
};

/*****************************************************************************
 * $conn-update-params                                                       *
 *****************************************************************************/

static int
cmd_conn_update_params(int argc, char **argv)
{
    struct ble_gap_upd_params params;
    uint16_t conn_handle;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    params.itvl_min = parse_arg_uint16_dflt(
        "itvl_min", BLE_GAP_INITIAL_CONN_ITVL_MIN, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_min' parameter\n");
        return rc;
    }

    params.itvl_max = parse_arg_uint16_dflt(
        "itvl_max", BLE_GAP_INITIAL_CONN_ITVL_MAX, &rc);
    if (rc != 0) {
        console_printf("invalid 'itvl_max' parameter\n");
        return rc;
    }

    params.latency = parse_arg_uint16_dflt("latency", 0, &rc);
    if (rc != 0) {
        console_printf("invalid 'latency' parameter\n");
        return rc;
    }

    params.supervision_timeout = parse_arg_uint16_dflt("timeout", 0x0100, &rc);
    if (rc != 0) {
        console_printf("invalid 'timeout' parameter\n");
        return rc;
    }

    params.min_ce_len = parse_arg_uint16_dflt("min_ce_len", 0x0010, &rc);
    if (rc != 0) {
        console_printf("invalid 'min_ce_len' parameter\n");
        return rc;
    }

    params.max_ce_len = parse_arg_uint16_dflt("max_ce_len", 0x0300, &rc);
    if (rc != 0) {
        console_printf("invalid 'max_ce_len' parameter\n");
        return rc;
    }

    rc = bletiny_update_conn(conn_handle, &params);
    if (rc != 0) {
        console_printf("error updating connection; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param conn_update_params_params[] = {
    {"conn", "conn_update_paramsion handle, usage: =<UINT16>"},
    {"interval_min", "usage: =[0-UINT16_MAX], default: 30"},
    {"interval_max", "usage: =[0-UINT16_MAX], default: 50"},
    {"latency", "usage: =[UINT16], default: 0"},
    {"timeout", "usage: =[UINT16], default: 0x0100"},
    {"min_conn_event_len", "usage: =[UINT16], default: 0x0010"},
    {"max_conn_event_len", "usage: =[UINT16], default: 0x0300"},
    {NULL, NULL}
};

static const struct shell_cmd_help conn_update_params_help = {
    .summary = "conn_update_params",
    .usage = "conn_update_params usage",
    .params = conn_update_params_params,
};

/*****************************************************************************
 * $conn-datalen                                                             *
 *****************************************************************************/

static int
cmd_conn_datalen(int argc, char **argv)
{
    uint16_t conn_handle;
    uint16_t tx_octets;
    uint16_t tx_time;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    tx_octets = parse_arg_long("octets", &rc);
    if (rc != 0) {
        console_printf("invalid 'octets' parameter\n");
        return rc;
    }

    tx_time = parse_arg_long("time", &rc);
    if (rc != 0) {
        console_printf("invalid 'time' parameter\n");
        return rc;
    }

    rc = bletiny_datalen(conn_handle, tx_octets, tx_time);
    if (rc != 0) {
        console_printf("error setting data length; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param conn_datalen_params[] = {
    {"conn", "conn_datalenion handle, usage: =<UINT16>"},
    {"octets", "usage: =<UINT16>"},
    {"time", "usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help conn_datalen_help = {
    .summary = "conn_datalen",
    .usage = "conn_datalen usage",
    .params = conn_datalen_params,
};

/*****************************************************************************
 * keystore                                                                  *
 *****************************************************************************/

static struct kv_pair cmd_keystore_entry_type[] = {
    { "msec",       BLE_STORE_OBJ_TYPE_PEER_SEC },
    { "ssec",       BLE_STORE_OBJ_TYPE_OUR_SEC },
    { "cccd",       BLE_STORE_OBJ_TYPE_CCCD },
    { NULL }
};

static int
cmd_keystore_parse_keydata(int argc, char **argv, union ble_store_key *out,
                           int *obj_type)
{
    int rc;

    memset(out, 0, sizeof(*out));
    *obj_type = parse_arg_kv("type", cmd_keystore_entry_type, &rc);
    if (rc != 0) {
        console_printf("invalid 'type' parameter\n");
        return rc;
    }

    switch (*obj_type) {
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
    case BLE_STORE_OBJ_TYPE_OUR_SEC:
        out->sec.peer_addr.type = parse_arg_kv("addr_type", cmd_addr_type, &rc);
        if (rc != 0) {
            console_printf("invalid 'addr_type' parameter\n");
            return rc;
        }

        rc = parse_arg_mac("addr", out->sec.peer_addr.val);
        if (rc != 0) {
            console_printf("invalid 'addr' parameter\n");
            return rc;
        }

        out->sec.ediv = parse_arg_uint16("ediv", &rc);
        if (rc != 0) {
            console_printf("invalid 'ediv' parameter\n");
            return rc;
        }

        out->sec.rand_num = parse_arg_uint64("rand", &rc);
        if (rc != 0) {
            console_printf("invalid 'rand' parameter\n");
            return rc;
        }
        return 0;

    default:
        return EINVAL;
    }
}

static int
cmd_keystore_parse_valuedata(int argc, char **argv,
                             int obj_type,
                             union ble_store_key *key,
                             union ble_store_value *out)
{
    int rc;
    int valcnt = 0;
    memset(out, 0, sizeof(*out));

    switch (obj_type) {
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
            rc = parse_arg_byte_stream_exact_length("ltk", out->sec.ltk, 16);
            if (rc == 0) {
                out->sec.ltk_present = 1;
                swap_in_place(out->sec.ltk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                console_printf("invalid 'ltk' parameter\n");
                return rc;
            }
            rc = parse_arg_byte_stream_exact_length("irk", out->sec.irk, 16);
            if (rc == 0) {
                out->sec.irk_present = 1;
                swap_in_place(out->sec.irk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                console_printf("invalid 'irk' parameter\n");
                return rc;
            }
            rc = parse_arg_byte_stream_exact_length("csrk", out->sec.csrk, 16);
            if (rc == 0) {
                out->sec.csrk_present = 1;
                swap_in_place(out->sec.csrk, 16);
                valcnt++;
            } else if (rc != ENOENT) {
                console_printf("invalid 'csrk' parameter\n");
                return rc;
            }
            out->sec.peer_addr = key->sec.peer_addr;
            out->sec.ediv = key->sec.ediv;
            out->sec.rand_num = key->sec.rand_num;
            break;
    }

    if (valcnt) {
        return 0;
    }
    return -1;
}

/*****************************************************************************
 * keystore-add                                                              *
 *****************************************************************************/

static int
cmd_keystore_add(int argc, char **argv)
{
    union ble_store_key key;
    union ble_store_value value;
    int obj_type;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    rc = cmd_keystore_parse_keydata(argc, argv, &key, &obj_type);

    if (rc) {
        return rc;
    }

    rc = cmd_keystore_parse_valuedata(argc, argv, obj_type, &key, &value);

    if (rc) {
        return rc;
    }

    switch(obj_type) {
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
            rc = ble_store_write_peer_sec(&value.sec);
            break;
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
            rc = ble_store_write_our_sec(&value.sec);
            break;
        case BLE_STORE_OBJ_TYPE_CCCD:
            rc = ble_store_write_cccd(&value.cccd);
            break;
        default:
            rc = ble_store_write(obj_type, &value);
    }
    return rc;
}

static const struct shell_param parse_keydata[] = {
    {"type", "entry type, usage: =<msec|ssec|cccd>"},
    {"addr_type", "usage: =<public|random>"},
    {"addr", "usage: =<XX:XX:XX:XX:XX:XX>"},
    {"ediv", "usage: =<UINT16>"},
    {"rand", "usage: =<UINT64>"},
    {NULL, NULL}
};

static const struct shell_param parse_valuedata[] = {
    {"ltk", "usage: =<XX:XX:...>, len=16 octets"},
    {"irk", "usage: =<XX:XX:...>, len=16 octets"},
    {"csrk", "usage: =<XX:XX:...>, len=16 octets"},
    {NULL, NULL}
};


static const struct shell_param keystore_add_params[] = {
    {"type", "entry type, usage: =<msec|ssec|cccd>"},
    {"addr_type", "usage: =<public|random>"},
    {"addr", "usage: =<XX:XX:XX:XX:XX:XX>"},
    {"ediv", "usage: =<UINT16>"},
    {"rand", "usage: =<UINT64>"},
    {"ltk", "usage: =<XX:XX:...>, len=16 octets"},
    {"irk", "usage: =<XX:XX:...>, len=16 octets"},
    {"csrk", "usage: =<XX:XX:...>, len=16 octets"},
    {NULL, NULL}
};

static const struct shell_cmd_help keystore_add_help = {
    .summary = "keystore_add",
    .usage = "keystore_add usage",
    .params = keystore_add_params,
};

/*****************************************************************************
 * keystore-del                                                              *
 *****************************************************************************/

static int
cmd_keystore_del(int argc, char **argv)
{
    union ble_store_key key;
    int obj_type;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    rc = cmd_keystore_parse_keydata(argc, argv, &key, &obj_type);

    if (rc) {
        return rc;
    }
    rc = ble_store_delete(obj_type, &key);
    return rc;
}

static const struct shell_param keystore_del_params[] = {
    {"type", "entry type, usage: =<msec|ssec|cccd>"},
    {"addr_type", "usage: =<public|random>"},
    {"addr", "usage: =<XX:XX:XX:XX:XX:XX>"},
    {"ediv", "usage: =<UINT16>"},
    {"rand", "usage: =<UINT64>"},
    {NULL, NULL}
};

static const struct shell_cmd_help keystore_del_help = {
    .summary = "keystore_del",
    .usage = "keystore_del usage",
    .params = keystore_del_params,
};

/*****************************************************************************
 * keystore-show                                                             *
 *****************************************************************************/

static int
cmd_keystore_iterator(int obj_type,
                      union ble_store_value *val,
                      void *cookie) {

    switch (obj_type) {
        case BLE_STORE_OBJ_TYPE_PEER_SEC:
        case BLE_STORE_OBJ_TYPE_OUR_SEC:
            console_printf("Key: ");
            if (ble_addr_cmp(&val->sec.peer_addr, BLE_ADDR_ANY) == 0) {
                console_printf("ediv=%u ", val->sec.ediv);
                console_printf("ediv=%llu ", val->sec.rand_num);
            } else {
                console_printf("addr_type=%u ", val->sec.peer_addr.type);
                print_addr(val->sec.peer_addr.val);
            }
            console_printf("\n");

            if (val->sec.ltk_present) {
                console_printf("    LTK: ");
                print_bytes(val->sec.ltk, 16);
                console_printf("\n");
            }
            if (val->sec.irk_present) {
                console_printf("    IRK: ");
                print_bytes(val->sec.irk, 16);
                console_printf("\n");
            }
            if (val->sec.csrk_present) {
                console_printf("    CSRK: ");
                print_bytes(val->sec.csrk, 16);
                console_printf("\n");
            }
            break;
    }
    return 0;
}

static int
cmd_keystore_show(int argc, char **argv)
{
    int type;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    type = parse_arg_kv("type", cmd_keystore_entry_type, &rc);
    if (rc != 0) {
        console_printf("invalid 'type' parameter\n");
        return rc;
    }

    ble_store_iterate(type, &cmd_keystore_iterator, NULL);
    return 0;
}

static const struct shell_param keystore_show_params[] = {
    {"type", "entry type, usage: =<msec|ssec|cccd>"},
    {NULL, NULL}
};

static const struct shell_cmd_help keystore_show_help = {
    .summary = "keystore_show",
    .usage = "keystore_show usage",
    .params = keystore_show_params,
};

#if NIMBLE_BLE_SM
/*****************************************************************************
 * $auth-passkey                                                             *
 *****************************************************************************/

static int
cmd_auth_passkey(int argc, char **argv)
{
#if !NIMBLE_BLE_SM
    return BLE_HS_ENOTSUP;
#endif

    uint16_t conn_handle;
    struct ble_sm_io pk;
    char *yesno;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    pk.action = parse_arg_uint16("action", &rc);
    if (rc != 0) {
        console_printf("invalid 'action' parameter\n");
        return rc;
    }

    switch (pk.action) {
        case BLE_SM_IOACT_INPUT:
        case BLE_SM_IOACT_DISP:
           /* passkey is 6 digit number */
           pk.passkey = parse_arg_long_bounds("key", 0, 999999, &rc);
           if (rc != 0) {
               console_printf("invalid 'key' parameter\n");
               return rc;
           }
           break;

        case BLE_SM_IOACT_OOB:
            rc = parse_arg_byte_stream_exact_length("oob", pk.oob, 16);
            if (rc != 0) {
                console_printf("invalid 'oob' parameter\n");
                return rc;
            }
            break;

        case BLE_SM_IOACT_NUMCMP:
            yesno = parse_arg_extract("yesno");
            if (yesno == NULL) {
                console_printf("invalid 'yesno' parameter\n");
                return EINVAL;
            }

            switch (yesno[0]) {
            case 'y':
            case 'Y':
                pk.numcmp_accept = 1;
                break;
            case 'n':
            case 'N':
                pk.numcmp_accept = 0;
                break;

            default:
                console_printf("invalid 'yesno' parameter\n");
                return EINVAL;
            }
            break;

       default:
         console_printf("invalid passkey action action=%d\n", pk.action);
         return EINVAL;
    }

    rc = ble_sm_inject_io(conn_handle, &pk);
    if (rc != 0) {
        console_printf("error providing passkey; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param auth_passkey_params[] = {
    {"conn", "auth_passkeyion handle, usage: =<UINT16>"},
    {"action", "usage: =<UINT16>"},
    {"key", "usage: =[0-999999]"},
    {"oob", "usage: =[XX:XX...], len=16 octets"},
    {"yesno", "usage: =[string]"},
    {NULL, NULL}
};

static const struct shell_cmd_help auth_passkey_help = {
    .summary = "auth_passkey",
    .usage = "auth_passkey usage",
    .params = auth_passkey_params,
};

/*****************************************************************************
 * $security-pair                                                            *
 *****************************************************************************/

static int
cmd_security_pair(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    rc = bletiny_sec_pair(conn_handle);
    if (rc != 0) {
        console_printf("error initiating pairing; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param security_pair_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help security_pair_help = {
    .summary = "security_pair",
    .usage = "security_pair usage",
    .params = security_pair_params,
};

/*****************************************************************************
 * $security-start                                                           *
 *****************************************************************************/

static int
cmd_security_start(int argc, char **argv)
{
    uint16_t conn_handle;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    rc = bletiny_sec_start(conn_handle);
    if (rc != 0) {
        console_printf("error starting security; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param security_start_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help security_start_help = {
    .summary = "security_start",
    .usage = "security_start usage",
    .params = security_start_params,
};

/*****************************************************************************
 * $security-encryption                                                      *
 *****************************************************************************/

static int
cmd_security_encryption(int argc, char **argv)
{
    uint16_t conn_handle;
    uint16_t ediv;
    uint64_t rand_val;
    uint8_t ltk[16];
    int rc;
    int auth;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    conn_handle = parse_arg_uint16("conn", &rc);
    if (rc != 0) {
        console_printf("invalid 'conn' parameter\n");
        return rc;
    }

    ediv = parse_arg_uint16("ediv", &rc);
    if (rc == ENOENT) {
        rc = bletiny_sec_restart(conn_handle, NULL, 0, 0, 0);
    } else {
        rand_val = parse_arg_uint64("rand", &rc);
        if (rc != 0) {
            console_printf("invalid 'rand' parameter\n");
            return rc;
        }

        auth = parse_arg_bool("auth", &rc);
        if (rc != 0) {
            console_printf("invalid 'auth' parameter\n");
            return rc;
        }

        rc = parse_arg_byte_stream_exact_length("ltk", ltk, 16);
        if (rc != 0) {
            console_printf("invalid 'ltk' parameter\n");
            return rc;
        }

        rc = bletiny_sec_restart(conn_handle, ltk, ediv, rand_val, auth);
    }

    if (rc != 0) {
        console_printf("error initiating encryption; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

static const struct shell_param security_encryption_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"ediv", "usage: =[UINT16]"},
    {"rand", "usage: =[UINT64]"},
    {"auth", "usage: =[0-1]"},
    {"ltk", "usage: =[XX:XX...], len=16 octets"},
    {NULL, NULL}
};

static const struct shell_cmd_help security_encryption_help = {
    .summary = "security_encryption",
    .usage = "security_encryption usage",
    .params = security_encryption_params,
};

/*****************************************************************************
 * $security-set-data                                                        *
 *****************************************************************************/

static int
cmd_security_set_data(int argc, char **argv)
{
    uint8_t tmp;
    int good;
    int rc;

    good = 0;

    tmp = parse_arg_bool("oob_flag", &rc);
    if (rc == 0) {
        ble_hs_cfg.sm_oob_data_flag = tmp;
        good++;
    } else if (rc != ENOENT) {
        console_printf("invalid 'oob_flag' parameter\n");
        return rc;
    }

    tmp = parse_arg_bool("mitm_flag", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_mitm = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'mitm_flag' parameter\n");
        return rc;
    }

    tmp = parse_arg_uint8("io_capabilities", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_io_cap = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'io_capabilities' parameter\n");
        return rc;
    }

    tmp = parse_arg_uint8("our_key_dist", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_our_key_dist = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'our_key_dist' parameter\n");
        return rc;
    }

    tmp = parse_arg_uint8("their_key_dist", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_their_key_dist = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'their_key_dist' parameter\n");
        return rc;
    }

    tmp = parse_arg_bool("bonding", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_bonding = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'bonding' parameter\n");
        return rc;
    }

    tmp = parse_arg_bool("sc", &rc);
    if (rc == 0) {
        good++;
        ble_hs_cfg.sm_sc = tmp;
    } else if (rc != ENOENT) {
        console_printf("invalid 'sc' parameter\n");
        return rc;
    }

    if (!good) {
        console_printf("Error: no valid settings specified\n");
        return -1;
    }

    return 0;
}

static const struct shell_param security_set_data_params[] = {
    {"oob_flag", "usage: =[0-1]"},
    {"mitm_flag", "usage: =[0-1]"},
    {"io_capabilities", "usage: =[UINT8]"},
    {"our_key_dist", "usage: =[UINT8]"},
    {"their_key_dist", "usage: =[UINT8]"},
    {"bonding", "usage: =[0-1]"},
    {"sc", "usage: =[0-1]"},
    {NULL, NULL}
};

static const struct shell_cmd_help security_set_data_help = {
    .summary = "security_set_data",
    .usage = "security_set_data usage",
    .params = security_set_data_params,
};
#endif

/*****************************************************************************
 * $test-tx                                                                  *
 *                                                                           *
 * Command to transmit 'num' packets of size 'len' at rate 'r' to
 * handle 'h' Note that length must be <= 251. The rate is in msecs.
 *
 *****************************************************************************/

static int
cmd_test_tx(int argc, char **argv)
{
    int rc;
    uint16_t rate;
    uint16_t len;
    uint16_t handle;
    uint16_t num;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    rate = parse_arg_uint16("rate", &rc);
    if (rc != 0) {
        console_printf("invalid 'rate' parameter\n");
        return rc;
    }

    len = parse_arg_uint16("length", &rc);
    if (rc != 0) {
        console_printf("invalid 'length' parameter\n");
        return rc;
    }
    if ((len > 251) || (len < 4)) {
        console_printf("error: len must be between 4 and 251, inclusive");
    }

    num = parse_arg_uint16("num", &rc);
    if (rc != 0) {
        console_printf("invalid 'num' parameter\n");
        return rc;
    }

    handle = parse_arg_uint16("handle", &rc);
    if (rc != 0) {
        console_printf("invalid 'handle' parameter\n");
        return rc;
    }

    rc = bletiny_tx_start(handle, len, rate, num);
    return rc;
}

static const struct shell_param test_tx_params[] = {
    {"num", "number of packets, usage: =<UINT16>"},
    {"length", "size of packet, usage: =<UINT16>"},
    {"rate", "rate of tx, usage: =<UINT16>"},
    {"handle", "handle to tx to, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help test_tx_help = {
    .summary = "test_tx",
    .usage = "test_tx usage",
    .params = test_tx_params,
};

/*****************************************************************************
 * $gatt-discover                                                            *
 *****************************************************************************/

static const struct shell_param gatt_discover_characteristic_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"uuid", "discover by uuid, usage: =[UUID]"},
    {"start", "start handle, usage: =<UINT16>"},
    {"end", "end handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_discover_characteristic_help = {
    .summary = "gatt_discover_characteristic",
    .usage = "gatt_discover_characteristic usage",
    .params = gatt_discover_characteristic_params,
};

static const struct shell_param gatt_discover_descriptor_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"start", "start handle, usage: =<UINT16>"},
    {"end", "end handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_discover_descriptor_help = {
    .summary = "gatt_discover_descriptor",
    .usage = "gatt_discover_descriptor usage",
    .params = gatt_discover_descriptor_params,
};

static const struct shell_param gatt_discover_service_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"uuid", "discover by uuid, usage: =[UUID]"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_discover_service_help = {
    .summary = "gatt_discover_service",
    .usage = "gatt_discover_service usage",
    .params = gatt_discover_service_params,
};

static const struct shell_param gatt_discover_full_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_discover_full_help = {
    .summary = "gatt_discover_full",
    .usage = "gatt_discover_full usage",
    .params = gatt_discover_full_params,
};

/*****************************************************************************
 * $gatt-exchange-mtu                                                        *
 *****************************************************************************/

static const struct shell_param gatt_exchange_mtu_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_exchange_mtu_help = {
    .summary = "gatt_exchange_mtu",
    .usage = "gatt_exchange_mtu usage",
    .params = gatt_exchange_mtu_params,
};

/*****************************************************************************
 * $gatt-find-included-services                                              *
 *****************************************************************************/

static const struct shell_param gatt_find_included_services_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"start", "start handle, usage: =<UINT16>"},
    {"end", "end handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_find_included_services_help = {
    .summary = "gatt_find_included_services",
    .usage = "gatt_find_included_services usage",
    .params = gatt_find_included_services_params,
};

/*****************************************************************************
 * $gatt-notify                                                                *
 *****************************************************************************/

static const struct shell_param gatt_notify_params[] = {
    {"attr", "attribute handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_notify_help = {
    .summary = "gatt_notify",
    .usage = "gatt_notify usage",
    .params = gatt_notify_params,
};

/*****************************************************************************
 * $gatt-read                                                                *
 *****************************************************************************/

static const struct shell_param gatt_read_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"long", "is read long, usage: =[0-1], default=0"},
    {"attr", "attribute handle, usage: =<UINT16>"},
    {"offset", "attribute handle, usage: =<UINT16>"},
    {"uuid", "read by uuid, usage: =[UUID]"},
    {"start", "start handle, usage: =<UINT16>"},
    {"end", "end handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_read_help = {
    .summary = "gatt_read",
    .usage = "gatt_read usage",
    .params = gatt_read_params,
};

/*****************************************************************************
 * $gatt-service-changed                                                     *
 *****************************************************************************/

static const struct shell_param gatt_service_changed_params[] = {
    {"start", "start handle, usage: =<UINT16>"},
    {"end", "end handle, usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_service_changed_help = {
    .summary = "gatt_service_changed",
    .usage = "gatt_service_changed usage",
    .params = gatt_service_changed_params,
};

/*****************************************************************************
 * $gatt-show                                                                *
 *****************************************************************************/

static const struct shell_param gatt_show_params[] = {
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_show_help = {
    .summary = "gatt_show",
    .usage = "gatt_show usage",
    .params = gatt_show_params,
};

static const struct shell_cmd_help gatt_show_addr_help = {
    .summary = "gatt_show_addr",
    .usage = "gatt_show addr usage",
    .params = gatt_show_params,
};

static const struct shell_cmd_help gatt_show_conn_help = {
    .summary = "gatt_show_conn",
    .usage = "gatt_show conn usage",
    .params = gatt_show_params,
};

static const struct shell_cmd_help gatt_show_coc_help = {
    .summary = "gatt_show_coc",
    .usage = "gatt_show coc usage",
    .params = gatt_show_params,
};

/*****************************************************************************
 * $gatt-write                                                                *
 *****************************************************************************/

static const struct shell_param gatt_write_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"no_rsp", "write without response, usage: =[0-1], default=0"},
    {"long", "is write long, usage: =[0-1], default=0"},
    {"attr", "attribute handle, usage: =<UINT16>"},
    {"offset", "attribute handle, usage: =<UINT16>"},
    {"value", "usage: =<octets>"},
    {NULL, NULL}
};

static const struct shell_cmd_help gatt_write_help = {
    .summary = "gatt_write",
    .usage = "gatt_write usage",
    .params = gatt_write_params,
};

/*****************************************************************************
 * $l2cap-update                                                             *
 *****************************************************************************/

static const struct shell_param l2cap_update_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"interval_min", "usage: =[0-UINT16_MAX], default: 30"},
    {"interval_max", "usage: =[0-UINT16_MAX], default: 50"},
    {"latency", "usage: =[UINT16], default: 0"},
    {"timeout", "usage: =[UINT16], default: 0x0100"},
    {NULL, NULL}
};

static const struct shell_cmd_help l2cap_update_help = {
    .summary = "l2cap_update",
    .usage = "l2cap_update usage",
    .params = l2cap_update_params,
};

/*****************************************************************************
 * $l2cap-create-server                                                      *
 *****************************************************************************/

static const struct shell_param l2cap_create_server_params[] = {
    {"psm", "usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help l2cap_create_server_help = {
    .summary = "l2cap_create_server",
    .usage = "l2cap_create_server usage",
    .params = l2cap_create_server_params,
};

/*****************************************************************************
 * $l2cap-connect                                                            *
 *****************************************************************************/

static const struct shell_param l2cap_connect_params[] = {
    {"conn", "connection handle, usage: =<UINT16>"},
    {"psm", "usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help l2cap_connect_help = {
    .summary = "l2cap_connect",
    .usage = "l2cap_connect usage",
    .params = l2cap_connect_params,
};

/*****************************************************************************
 * $l2cap-disconnect                                                         *
 *****************************************************************************/

static const struct shell_param l2cap_disconnect_params[] = {
    {"conn", "disconnection handle, usage: =<UINT16>"},
    {"idx", "usage: =<UINT16>"},
    {NULL, NULL}
};

static const struct shell_cmd_help l2cap_disconnect_help = {
    .summary = "l2cap_disconnect",
    .usage = "l2cap_disconnect usage, use show-coc to get the parameters",
    .params = l2cap_disconnect_params,
};


static const struct shell_cmd btshell_commands[] = {
    {
        .cmd_name = "advertise",
        .cb = cmd_advertise,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &advertise_help,
#endif
    },
    {
        .cmd_name = "connect",
        .cb = cmd_connect,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &connect_help,
#endif
    },
    {
        .cmd_name = "disconnect",
        .cb = cmd_disconnect,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &disconnect_help,
#endif
    },
    {
        .cmd_name = "scan",
        .cb = cmd_scan,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &scan_help,
#endif
    },
    {
        .cmd_name = "set",
        .cb = cmd_set,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &set_help,
#endif
    },
    {
        .cmd_name = "set-adv-data",
        .cb = cmd_set_adv_data,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &set_adv_data_help,
#endif
    },
    {
        .cmd_name = "white-list",
        .cb = cmd_white_list,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &white_list_help,
#endif
    },
    {
        .cmd_name = "conn-rssi",
        .cb = cmd_conn_rssi,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &conn_rssi_help,
#endif
    },
    {
        .cmd_name = "conn-update-params",
        .cb = cmd_conn_update_params,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &conn_update_params_help,
#endif
    },
    {
        .cmd_name = "conn-datalen",
        .cb = cmd_conn_datalen,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &conn_datalen_help,
#endif
    },
    {
        .cmd_name = "gatt-discover-characteristic",
        .cb = cmd_gatt_discover_characteristic,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_discover_characteristic_help,
#endif
    },
    {
        .cmd_name = "gatt-discover-descriptor",
        .cb = cmd_gatt_discover_descriptor,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_discover_descriptor_help,
#endif
    },
    {
        .cmd_name = "gatt-discover-service",
        .cb = cmd_gatt_discover_service,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_discover_service_help,
#endif
    },
    {
        .cmd_name = "gatt-discover-full",
        .cb = cmd_gatt_discover_full,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_discover_full_help,
#endif
    },
    {
        .cmd_name = "gatt-find-included-services",
        .cb = cmd_gatt_find_included_services,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_find_included_services_help,
#endif
    },
    {
        .cmd_name = "gatt-exchange-mtu",
        .cb = cmd_gatt_exchange_mtu,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_exchange_mtu_help,
#endif
    },
    {
        .cmd_name = "gatt-read",
        .cb = cmd_gatt_read,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_read_help,
#endif
    },
    {
        .cmd_name = "gatt-notify",
        .cb = cmd_gatt_notify,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_notify_help,
#endif
    },
    {
        .cmd_name = "gatt-service-changed",
        .cb = cmd_gatt_service_changed,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_service_changed_help,
#endif
    },
    {
        .cmd_name = "gatt-show",
        .cb = cmd_gatt_show,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_show_help,
#endif
    },
    {
        .cmd_name = "gatt-show-addr",
        .cb = cmd_gatt_show_addr,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_show_addr_help,
#endif
    },
    {
        .cmd_name = "gatt-show-conn",
        .cb = cmd_gatt_show_conn,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_show_conn_help,
#endif
    },
    {
        .cmd_name = "gatt-show-coc",
        .cb = cmd_gatt_show_coc,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_show_coc_help,
#endif
    },
    {
        .cmd_name = "gatt-write",
        .cb = cmd_gatt_write,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &gatt_write_help,
#endif
    },
#if MYNEWT_VAL(BLE_L2CAP_COC_MAX_NUM)
    {
        .cmd_name = "l2cap-update",
        .cb = cmd_l2cap_update,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &l2cap_update_help,
#endif
    },
    {
        .cmd_name = "l2cap-create-server",
        .cb = cmd_l2cap_create_server,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &l2cap_create_server_help,
#endif
    },
    {
        .cmd_name = "l2cap-connect",
        .cb = cmd_l2cap_connect,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &l2cap_connect_help,
#endif
    },
    {
        .cmd_name = "l2cap-disconnect",
        .cb = cmd_l2cap_disconnect,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &l2cap_disconnect_help,
#endif
    },
#endif
    {
        .cmd_name = "keystore-add",
        .cb = cmd_keystore_add,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &keystore_add_help,
#endif
    },
    {
        .cmd_name = "keystore-del",
        .cb = cmd_keystore_del,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &keystore_del_help,
#endif
    },
    {
        .cmd_name = "keystore-show",
        .cb = cmd_keystore_show,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &keystore_show_help,
#endif
    },
#if NIMBLE_BLE_SM
    {
        .cmd_name = "auth-passkey",
        .cb = cmd_auth_passkey,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &auth_passkey_help,
#endif
    },
    {
        .cmd_name = "security-pair",
        .cb = cmd_security_pair,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &security_pair_help,
#endif
    },
    {
        .cmd_name = "security-start",
        .cb = cmd_security_start,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &security_start_help,
#endif
    },
    {
        .cmd_name = "security-encryption",
        .cb = cmd_security_encryption,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &security_encryption_help,
#endif
    },
    {
        .cmd_name = "security-set-data",
        .cb = cmd_security_set_data,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &set_sm_data_help,
#endif
    },
#endif
    {
        .cmd_name = "test-tx",
        .cb = cmd_test_tx,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        .help = &test_tx_help,
#endif
    },
    { NULL, NULL, NULL },
};


void
cmd_init(void)
{
    shell_register(BTSHELL_MODULE, btshell_commands);
    shell_register_default_module(BTSHELL_MODULE);
}
