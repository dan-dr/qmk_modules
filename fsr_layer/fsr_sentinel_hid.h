// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* QMK Raw HID, 32-byte reports, usage page 0xFF60 / usage 0x61.
 * Prefix 0x91 is distinct from Argos 0x90 and KeyPeek 0xC0.
 * All replies retain prefix and command in bytes 0 and 1.
 *
 * GET_INFO reply: schema, algorithm_count, active_id, active_param_count,
 *                 generation_u16_le
 * GET_ALGORITHM req: index
 *               reply: index, status, id, param_count, key\0
 * SET_ALGORITHM req: id
 *               reply: active_id, status, generation_u16_le
 * GET_PARAM req: algorithm_id, index
 *           reply: algorithm_id, index, status, min, max, default, value,
 *                  key\0. Numeric fields are int16 little-endian.
 * SET_PARAM req: algorithm_id, index, value_i16_le
 *           reply: same as GET_PARAM
 * GET_ALL req: algorithm_id
 *         reply: algorithm_id, returned_count, up to 14 int16 values
 * GET_ALL_PAGE req: algorithm_id, start_index
 *              reply: algorithm_id, start_index, returned_count,
 *                     up to 13 int16 values
 * RESET_DEFAULTS req: algorithm_id
 *                reply: same as GET_ALL_PAGE with start_index 0
 * GET_DEBUG reply: enabled, compact, interval_ms_u16_le
 * SET_DEBUG req: enabled
 *           reply: same as GET_DEBUG. Sets QMK debug_enable (same flag as
 *                  DB_TOGG). While enabled, firmware also pushes DEBUG_SAMPLE
 *                  reports about every FSR_DEBUG_INTERVAL_MS.
 * DEBUG_SAMPLE (unsolicited): scan_u32, t_ms_u32, reading_i16, filtered_i16,
 *                             mx_i16, my_i16, witness_i16, touch, state, alg,
 *                             generation_u16
 */
#define FSR_SENTINEL_HID_PREFIX 0x91

enum {
    fsr_sentinel_hid_get_info = 0x01,
    fsr_sentinel_hid_get_param = 0x02,
    fsr_sentinel_hid_set_param = 0x03,
    fsr_sentinel_hid_get_all = 0x04,
    fsr_sentinel_hid_reset_defaults = 0x05,
    fsr_sentinel_hid_get_algorithm = 0x06,
    fsr_sentinel_hid_set_algorithm = 0x07,
    fsr_sentinel_hid_get_all_page = 0x08,
    fsr_sentinel_hid_get_debug = 0x09,
    fsr_sentinel_hid_set_debug = 0x0A,
    fsr_sentinel_hid_debug_sample = 0x10,
};

bool fsr_sentinel_handle_command(uint8_t *data, uint8_t length);
void fsr_sentinel_hid_send_sample(uint32_t scan, uint32_t t_ms, int16_t reading,
                                  int16_t filtered, int16_t motion_x,
                                  int16_t motion_y, int16_t witness,
                                  uint8_t touched, uint8_t state,
                                  uint8_t algorithm, uint16_t generation);
