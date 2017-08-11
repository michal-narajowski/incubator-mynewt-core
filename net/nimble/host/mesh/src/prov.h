/*  Bluetooth Mesh */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PROV_H__
#define __PROV_H__

#include "os/os_mbuf.h"
#include "mesh.h"
#include "../src/ble_hs_conn_priv.h"

void
bt_mesh_pb_adv_recv(struct os_mbuf *buf);

bool
bt_prov_active(void);

int
bt_mesh_pb_gatt_open(struct ble_hs_conn *conn);
int
bt_mesh_pb_gatt_close(struct ble_hs_conn *conn);
int
bt_mesh_pb_gatt_recv(struct ble_hs_conn *conn, struct os_mbuf *buf);

const u8_t *
bt_mesh_prov_get_uuid(void);

void
bt_mesh_prov_init(const struct bt_mesh_prov *prov);

#endif
