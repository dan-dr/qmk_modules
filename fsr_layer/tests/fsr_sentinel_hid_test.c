// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../fsr_layer.h"
#include "../fsr_sentinel_hid.h"
#include "../fsr_sentinel_runtime.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned send_count;
bool debug_enable;

void raw_hid_send(uint8_t *data, uint8_t length) {
    assert(data != NULL);
    assert(length == 32U);
    send_count++;
}

static void command(uint8_t *data, uint8_t command_id) {
    memset(data, 0, 32U);
    data[0] = FSR_SENTINEL_HID_PREFIX;
    data[1] = command_id;
}

int main(void) {
    uint8_t data[32];
    command(data, fsr_sentinel_hid_get_info);
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(send_count == 1U);
    assert(data[2] == FSR_SENTINEL_RUNTIME_SCHEMA_VERSION);
    assert(data[3] == FSR_SENTINEL_ALGORITHM_COUNT);
    assert(data[4] == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1);

    command(data, fsr_sentinel_hid_get_algorithm);
    data[2] = 1;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[3] == 1U);
    assert(data[4] == FSR_SENTINEL_ALGORITHM_V2);
    assert(data[5] == 9U);
    assert(strcmp((char *)&data[6], "v2_signal_envelope") == 0);

    command(data, fsr_sentinel_hid_get_algorithm);
    data[2] = 3;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[3] == 1U);
    assert(data[4] == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1);
    assert(data[5] == FSR_ATLAS_PHASE_PARAM_COUNT);
    assert(strcmp((char *)&data[6], "atlas_phase_v1") == 0);

    command(data, fsr_sentinel_hid_set_algorithm);
    data[2] = FSR_SENTINEL_ALGORITHM_V2;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[2] == FSR_SENTINEL_ALGORITHM_V2);
    assert(data[3] == 1U);

    command(data, fsr_sentinel_hid_set_param);
    data[2] = FSR_SENTINEL_ALGORITHM_V2;
    data[3] = 1;
    data[4] = 123;
    data[5] = 0;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[4] == 1U);
    assert(data[11] == 123U);
    assert(data[12] == 0U);

    command(data, fsr_sentinel_hid_set_algorithm);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[2] == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1);
    assert(data[3] == 1U);

    command(data, fsr_sentinel_hid_set_param);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    data[3] = FSR_ATLAS_PARAM_RETOUCH_RISE;
    data[4] = 41;
    data[5] = 0;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[4] == 1U);
    assert(data[11] == 41U);

    command(data, fsr_sentinel_hid_set_param);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    data[3] = FSR_ATLAS_PARAM_FRONTEND;
    data[4] = 5U;
    data[5] = 0U;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[4] == 0U);
    assert(data[11] == FSR_ATLAS_FRONTEND_RAW);

    command(data, fsr_sentinel_hid_set_param);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    data[3] = FSR_ATLAS_PARAM_RELEASE_RESIDUAL;
    data[4] = 0x0CU;
    data[5] = 0xFEU; /* -500, signed lower bound */
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[4] == 1U);
    assert(data[11] == 0x0CU);
    assert(data[12] == 0xFEU);

    command(data, fsr_sentinel_hid_get_all);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[2] == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1);
    assert(data[3] == 14U);

    command(data, fsr_sentinel_hid_get_all_page);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    data[3] = 13U;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[2] == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1);
    assert(data[3] == 13U);
    assert(data[4] == 4U);
    assert(data[11] == 41U);

    command(data, fsr_sentinel_hid_get_all_page);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    data[3] = 17U;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[4] == 0U);

    command(data, fsr_sentinel_hid_reset_defaults);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    command(data, fsr_sentinel_hid_get_param);
    data[2] = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
    data[3] = FSR_ATLAS_PARAM_RETOUCH_RISE;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[11] == 40U);

    debug_enable = false;
    command(data, fsr_sentinel_hid_get_debug);
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[2] == 0U);

    command(data, fsr_sentinel_hid_set_debug);
    data[2] = 1;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(debug_enable);
    assert(data[2] == 1U);
    assert(data[4] == (uint8_t)(FSR_DEBUG_INTERVAL_MS & 0xFF));

    command(data, fsr_sentinel_hid_get_info);
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(data[8] == 1U);

    unsigned streamed = send_count;
    fsr_sentinel_hid_send_sample(7, 1234, 1987, 1980, -3, 4, 12, 1, 1, 4, 9);
    assert(send_count == streamed + 1U);

    command(data, fsr_sentinel_hid_set_debug);
    data[2] = 0;
    assert(fsr_sentinel_handle_command(data, sizeof(data)));
    assert(!debug_enable);
    streamed = send_count;
    fsr_sentinel_hid_send_sample(8, 1235, 10, 10, 0, 0, 0, 0, 0, 4, 9);
    assert(send_count == streamed);

    unsigned before = send_count;
    assert(!fsr_sentinel_handle_command(data, 31U));
    assert(send_count == before);
    puts("fsr_sentinel_hid_test: ok");
    return 0;
}
