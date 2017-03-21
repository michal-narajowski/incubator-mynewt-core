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
    {"irk", "Identity Resolving Key, usage: =[XX:XX:XX:XX:XX:XX:XX:XX]"},
    {NULL, NULL}
};

static const struct shell_cmd_help set_help = {
    .summary = "set",
    .usage = "set usage",
    .params = set_params,
};

static const struct shell_cmd btshell_commands[] = {
    {
        .cmd_name = "advertise",
        .cb = cmd_advertise,
        .help = &advertise_help,
    },
    {
        .cmd_name = "connect",
        .cb = cmd_connect,
        .help = &connect_help,
    },
    {
        .cmd_name = "disconnect",
        .cb = cmd_disconnect,
        .help = &disconnect_help,
    },
    {
        .cmd_name = "scan",
        .cb = cmd_scan,
        .help = &scan_help,
    },
    {
        .cmd_name = "set",
        .cb = cmd_set,
        .help = &set_help,
    },
    { NULL, NULL, NULL },
};


void
cmd_init(void)
{
    shell_register(BTSHELL_MODULE, btshell_commands);
    shell_register_default_module(BTSHELL_MODULE);
}
