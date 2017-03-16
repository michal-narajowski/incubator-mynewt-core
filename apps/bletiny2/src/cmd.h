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

#ifndef CMD_H
#define CMD_H

#include <inttypes.h>
#include "host/ble_uuid.h"

typedef int cmd_fn(int argc, char **argv);
struct cmd_entry {
    char *name;
    cmd_fn *cb;
};

struct kv_pair {
    char *key;
    int val;
};

const struct cmd_entry *parse_cmd_find(const struct cmd_entry *cmds,
                                       char *name);
struct kv_pair *parse_kv_find(struct kv_pair *kvs, char *name);
int parse_arg_find_idx(const char *key);
char *parse_arg_extract(const char *key);
long parse_arg_long_bounds(char *name, long min, long max, int *out_status);
long parse_arg_long_bounds_default(char *name, long min, long max,
                                   long dflt, int *out_status);
uint64_t parse_arg_uint64_bounds(char *name, uint64_t min,
                                 uint64_t max, int *out_status);
long parse_arg_long(char *name, int *staus);
uint8_t parse_arg_bool(char *name, int *status);
uint8_t parse_arg_bool_default(char *name, uint8_t dflt, int *out_status);
uint8_t parse_arg_uint8(char *name, int *status);
uint8_t parse_arg_uint8_dflt(char *name, uint8_t dflt, int *out_status);
uint16_t parse_arg_uint16(char *name, int *status);
uint16_t parse_arg_uint16_dflt(char *name, uint16_t dflt, int *out_status);
uint32_t parse_arg_uint32(char *name, int *out_status);
uint32_t parse_arg_uint32_dflt(char *name, uint32_t dflt, int *out_status);
uint64_t parse_arg_uint64(char *name, int *out_status);
int parse_arg_kv(char *name, struct kv_pair *kvs, int *out_status);
int parse_arg_kv_default(char *name, struct kv_pair *kvs, int def_val,
                         int *out_status);
int parse_arg_byte_stream(char *name, int max_len, uint8_t *dst, int *out_len);
int parse_arg_byte_stream_exact_length(char *name, uint8_t *dst, int len);
int parse_arg_mac(char *name, uint8_t *dst);
int parse_arg_uuid(char *name, ble_uuid_any_t *uuid);
int parse_err_too_few_args(char *cmd_name);
int parse_arg_all(int argc, char **argv);
void cmd_init(void);

#endif