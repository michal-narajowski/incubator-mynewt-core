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

#include <inttypes.h>
#include <errno.h>

#include "console/console.h"
#include "bletiny.h"
#include "cmd.h"
#include "cmd_gatt.h"


/*****************************************************************************
 * $gatt-discover                                                            *
 *****************************************************************************/

int
cmd_gatt_discover_characteristic(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    ble_uuid_any_t uuid;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        console_printf("invalid 'conn start end' parameter\n");
        return rc;
    }

    rc = parse_arg_uuid("uuid", &uuid);
    if (rc == 0) {
        rc = bletiny_disc_chrs_by_uuid(conn_handle, start_handle, end_handle,
                                       &uuid.u);
    } else if (rc == ENOENT) {
        rc = bletiny_disc_all_chrs(conn_handle, start_handle, end_handle);
    } else  {
        console_printf("invalid 'uuid' parameter\n");
        return rc;
    }
    if (rc != 0) {
        console_printf("error discovering characteristics; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

int
cmd_gatt_discover_descriptor(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        console_printf("invalid 'conn start end' parameter\n");
        return rc;
    }

    rc = bletiny_disc_all_dscs(conn_handle, start_handle, end_handle);
    if (rc != 0) {
        console_printf("error discovering descriptors; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

int
cmd_gatt_discover_service(int argc, char **argv)
{
    ble_uuid_any_t uuid;
    int conn_handle;
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

    rc = parse_arg_uuid("uuid", &uuid);
    if (rc == 0) {
        rc = bletiny_disc_svc_by_uuid(conn_handle, &uuid.u);
    } else if (rc == ENOENT) {
        rc = bletiny_disc_svcs(conn_handle);
    } else  {
        console_printf("invalid 'uuid' parameter\n");
        return rc;
    }

    if (rc != 0) {
        console_printf("error discovering services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

int
cmd_gatt_discover_full(int argc, char **argv)
{
    int conn_handle;
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

    rc = bletiny_disc_full(conn_handle);
    if (rc != 0) {
        console_printf("error discovering all; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $gatt-exchange-mtu                                                        *
 *****************************************************************************/

int
cmd_gatt_exchange_mtu(int argc, char **argv)
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

    rc = bletiny_exchange_mtu(conn_handle);
    if (rc != 0) {
        console_printf("error exchanging mtu; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $gatt-read                                                                *
 *****************************************************************************/

#define CMD_READ_MAX_ATTRS  8

int
cmd_gatt_read(int argc, char **argv)
{
    static uint16_t attr_handles[CMD_READ_MAX_ATTRS];
    uint16_t conn_handle;
    uint16_t start;
    uint16_t end;
    uint16_t offset;
    ble_uuid_any_t uuid;
    uint8_t num_attr_handles;
    int is_uuid;
    int is_long;
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

    is_long = parse_arg_long("long", &rc);
    if (rc == ENOENT) {
        is_long = 0;
    } else if (rc != 0) {
        console_printf("invalid 'long' parameter\n");
        return rc;
    }

    for (num_attr_handles = 0;
         num_attr_handles < CMD_READ_MAX_ATTRS;
         num_attr_handles++) {

        attr_handles[num_attr_handles] = parse_arg_uint16("attr", &rc);
        if (rc == ENOENT) {
            break;
        } else if (rc != 0) {
            console_printf("invalid 'attr' parameter\n");
            return rc;
        }
    }

    rc = parse_arg_uuid("uuid", &uuid);
    if (rc == ENOENT) {
        is_uuid = 0;
    } else if (rc == 0) {
        is_uuid = 1;
    } else {
        console_printf("invalid 'uuid' parameter\n");
        return rc;
    }

    start = parse_arg_uint16("start", &rc);
    if (rc == ENOENT) {
        start = 0;
    } else if (rc != 0) {
        console_printf("invalid 'start' parameter\n");
        return rc;
    }

    end = parse_arg_uint16("end", &rc);
    if (rc == ENOENT) {
        end = 0;
    } else if (rc != 0) {
        console_printf("invalid 'end' parameter\n");
        return rc;
    }

    offset = parse_arg_uint16("offset", &rc);
    if (rc == ENOENT) {
        offset = 0;
    } else if (rc != 0) {
        console_printf("invalid 'offset' parameter\n");
        return rc;
    }

    if (num_attr_handles == 1) {
        if (is_long) {
            rc = bletiny_read_long(conn_handle, attr_handles[0], offset);
        } else {
            rc = bletiny_read(conn_handle, attr_handles[0]);
        }
    } else if (num_attr_handles > 1) {
        rc = bletiny_read_mult(conn_handle, attr_handles, num_attr_handles);
    } else if (is_uuid) {
        if (start == 0 || end == 0) {
            rc = EINVAL;
        } else {
            rc = bletiny_read_by_uuid(conn_handle, start, end, &uuid.u);
        }
    } else {
        rc = EINVAL;
    }

    if (rc != 0) {
        console_printf("error reading characteristic; rc=%d\n", rc);
        return rc;
    }

    return 0;
}

/*****************************************************************************
 * $gatt-find-included-services                                              *
 *****************************************************************************/

int
cmd_gatt_find_included_services(int argc, char **argv)
{
    uint16_t start_handle;
    uint16_t conn_handle;
    uint16_t end_handle;
    int rc;

    rc = parse_arg_all(argc - 1, argv + 1);
    if (rc != 0) {
        return rc;
    }

    rc = cmd_parse_conn_start_end(&conn_handle, &start_handle, &end_handle);
    if (rc != 0) {
        console_printf("invalid 'conn start end' parameter\n");
        return rc;
    }

    rc = bletiny_find_inc_svcs(conn_handle, start_handle, end_handle);
    if (rc != 0) {
        console_printf("error finding included services; rc=%d\n", rc);
        return rc;
    }

    return 0;
}
