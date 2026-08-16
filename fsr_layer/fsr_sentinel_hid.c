// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fsr_sentinel_hid.h"

#include "fsr_layer.h"
#include "fsr_sentinel_runtime.h"

#include <string.h>

#ifdef FSR_ENABLE
#    include "debug.h"
#    include "raw_hid.h"
#endif

#ifndef RAW_EPSIZE
#    define RAW_EPSIZE 32
#endif

#ifdef FSR_ENABLE
static void write_i16(uint8_t *dest, int16_t value) {
    dest[0] = (uint8_t)(value & 0xFF);
    dest[1] = (uint8_t)((value >> 8) & 0xFF);
}

static int16_t read_i16(const uint8_t *src) {
    return (int16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static void write_key(uint8_t *dest, size_t room, const char *key) {
    if (room == 0U) {
        return;
    }
    size_t length = strlen(key);
    if (length >= room) {
        length = room - 1U;
    }
    memcpy(dest, key, length);
    dest[length] = '\0';
}

static void write_algorithm(uint8_t *payload, uint8_t index) {
    memset(payload, 0, RAW_EPSIZE - 2U);
    const fsr_sentinel_algorithm_meta_t *meta =
        fsr_sentinel_runtime_algorithm_meta(index);
    payload[0] = index;
    if (meta == NULL) {
        return;
    }
    payload[1] = 1;
    payload[2] = meta->id;
    payload[3] = meta->param_count;
    write_key(&payload[4], RAW_EPSIZE - 6U, meta->key);
}

static void write_param(uint8_t *payload, uint8_t algorithm, uint8_t index,
                        int16_t value) {
    memset(payload, 0, RAW_EPSIZE - 2U);
    const fsr_sentinel_param_meta_t *meta =
        fsr_sentinel_runtime_param_meta(algorithm, index);
    payload[0] = algorithm;
    payload[1] = index;
    if (meta == NULL) {
        return;
    }
    payload[2] = 1;
    write_i16(&payload[3], meta->min);
    write_i16(&payload[5], meta->max);
    write_i16(&payload[7], meta->default_value);
    write_i16(&payload[9], value);
    write_key(&payload[11], RAW_EPSIZE - 13U, meta->key);
}

static void write_all(uint8_t *payload, uint8_t algorithm) {
    memset(payload, 0, RAW_EPSIZE - 2U);
    uint8_t count = fsr_sentinel_runtime_param_count(algorithm);
    if (count > 14U) {
        count = 14U;
    }
    payload[0] = algorithm;
    payload[1] = count;
    for (uint8_t index = 0; index < count; index++) {
        int16_t value = 0;
        fsr_sentinel_runtime_param_get(algorithm, index, &value);
        write_i16(&payload[2U + index * 2U], value);
    }
}

static void write_all_page(uint8_t *payload, uint8_t algorithm,
                           uint8_t start) {
    memset(payload, 0, RAW_EPSIZE - 2U);
    uint8_t total = fsr_sentinel_runtime_param_count(algorithm);
    uint8_t count = start < total ? (uint8_t)(total - start) : 0U;
    if (count > 13U) {
        count = 13U;
    }
    payload[0] = algorithm;
    payload[1] = start;
    payload[2] = count;
    for (uint8_t offset = 0; offset < count; offset++) {
        int16_t value = 0;
        fsr_sentinel_runtime_param_get(algorithm, (uint8_t)(start + offset),
                                       &value);
        write_i16(&payload[3U + offset * 2U], value);
    }
}

static void write_u32(uint8_t *dest, uint32_t value) {
    dest[0] = (uint8_t)(value & 0xFFU);
    dest[1] = (uint8_t)((value >> 8) & 0xFFU);
    dest[2] = (uint8_t)((value >> 16) & 0xFFU);
    dest[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static void write_debug(uint8_t *payload) {
    memset(payload, 0, RAW_EPSIZE - 2U);
    payload[0] = debug_enable ? 1U : 0U;
    payload[1] = FSR_DEBUG_COMPACT_ENABLED;
    write_i16(&payload[2], (int16_t)FSR_DEBUG_INTERVAL_MS);
}
#endif

bool fsr_sentinel_handle_command(uint8_t *data, uint8_t length) {
#ifndef FSR_ENABLE
    (void)data;
    (void)length;
    return false;
#else
    if (data == NULL || length < RAW_EPSIZE ||
        data[0] != FSR_SENTINEL_HID_PREFIX) {
        return false;
    }

    uint8_t command = data[1];
    uint8_t *payload = &data[2];
    switch (command) {
        case fsr_sentinel_hid_get_info: {
            memset(payload, 0, RAW_EPSIZE - 2U);
            payload[0] = FSR_SENTINEL_RUNTIME_SCHEMA_VERSION;
            payload[1] = FSR_SENTINEL_ALGORITHM_COUNT;
            payload[2] = fsr_sentinel_runtime_active_algorithm();
            payload[3] = fsr_sentinel_runtime_param_count(payload[2]);
            uint16_t generation = fsr_sentinel_runtime_generation();
            payload[4] = (uint8_t)(generation & 0xFFU);
            payload[5] = (uint8_t)(generation >> 8);
            payload[6] = debug_enable ? 1U : 0U;
            break;
        }
        case fsr_sentinel_hid_get_algorithm:
            write_algorithm(payload, payload[0]);
            break;
        case fsr_sentinel_hid_set_algorithm: {
            uint8_t algorithm = payload[0];
            bool ok = fsr_sentinel_runtime_set_algorithm(algorithm);
            memset(payload, 0, RAW_EPSIZE - 2U);
            payload[0] = fsr_sentinel_runtime_active_algorithm();
            payload[1] = ok ? 1U : 0U;
            uint16_t generation = fsr_sentinel_runtime_generation();
            payload[2] = (uint8_t)(generation & 0xFFU);
            payload[3] = (uint8_t)(generation >> 8);
            break;
        }
        case fsr_sentinel_hid_get_param: {
            uint8_t algorithm = payload[0];
            uint8_t index = payload[1];
            int16_t value = 0;
            fsr_sentinel_runtime_param_get(algorithm, index, &value);
            write_param(payload, algorithm, index, value);
            break;
        }
        case fsr_sentinel_hid_set_param: {
            uint8_t algorithm = payload[0];
            uint8_t index = payload[1];
            int16_t value = read_i16(&payload[2]);
            bool ok = fsr_sentinel_runtime_param_set(algorithm, index, value);
            int16_t stored = value;
            fsr_sentinel_runtime_param_get(algorithm, index, &stored);
            write_param(payload, algorithm, index, stored);
            if (!ok) {
                payload[2] = 0;
            }
            break;
        }
        case fsr_sentinel_hid_get_all:
            write_all(payload, payload[0]);
            break;
        case fsr_sentinel_hid_get_all_page: {
            uint8_t algorithm = payload[0];
            uint8_t start = payload[1];
            write_all_page(payload, algorithm, start);
            break;
        }
        case fsr_sentinel_hid_reset_defaults: {
            uint8_t algorithm = payload[0];
            fsr_sentinel_runtime_params_reset(algorithm);
            write_all_page(payload, algorithm, 0U);
            break;
        }
        case fsr_sentinel_hid_get_debug:
            write_debug(payload);
            break;
        case fsr_sentinel_hid_set_debug:
            debug_enable = payload[0] != 0U;
            write_debug(payload);
            break;
        default:
            return true;
    }

    raw_hid_send(data, length);
    return true;
#endif
}

void fsr_sentinel_hid_send_sample(uint32_t scan, uint32_t t_ms, int16_t reading,
                                  int16_t filtered, int16_t motion_x,
                                  int16_t motion_y, int16_t witness,
                                  uint8_t touched, uint8_t state,
                                  uint8_t algorithm, uint16_t generation) {
#ifndef FSR_ENABLE
    (void)scan;
    (void)t_ms;
    (void)reading;
    (void)filtered;
    (void)motion_x;
    (void)motion_y;
    (void)witness;
    (void)touched;
    (void)state;
    (void)algorithm;
    (void)generation;
#else
    if (!debug_enable) {
        return;
    }
    uint8_t data[RAW_EPSIZE];
    memset(data, 0, sizeof(data));
    data[0] = FSR_SENTINEL_HID_PREFIX;
    data[1] = fsr_sentinel_hid_debug_sample;
    write_u32(&data[2], scan);
    write_u32(&data[6], t_ms);
    write_i16(&data[10], reading);
    write_i16(&data[12], filtered);
    write_i16(&data[14], motion_x);
    write_i16(&data[16], motion_y);
    write_i16(&data[18], witness);
    data[20] = touched;
    data[21] = state;
    data[22] = algorithm;
    data[23] = (uint8_t)(generation & 0xFFU);
    data[24] = (uint8_t)(generation >> 8);
    raw_hid_send(data, RAW_EPSIZE);
#endif
}
