// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "fsr_atlas_phase.h"
#include "fsr_sentinel.h"

#include <stdbool.h>
#include <stdint.h>

#define FSR_SENTINEL_RUNTIME_SCHEMA_VERSION 5U
#define FSR_SENTINEL_RUNTIME_STATE_BUDGET_BYTES 384U

typedef enum {
    FSR_SENTINEL_ALGORITHM_V1 = 1,
    FSR_SENTINEL_ALGORITHM_V2 = 2,
    FSR_SENTINEL_ALGORITHM_V3 = 3,
    FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1 = 4,
    FSR_SENTINEL_ALGORITHM_COUNT = 4,
} fsr_sentinel_algorithm_id_t;

typedef struct {
    uint8_t id;
    const char *key;
    uint8_t param_count;
} fsr_sentinel_algorithm_meta_t;

typedef struct {
    uint16_t center_rate;
    uint16_t touch_delta;
    uint16_t touch_confirm_ms;
    uint16_t peak_decay;
    uint16_t release_drop;
    int16_t release_floor_margin;
    uint16_t release_confirm_ms;
    uint16_t recovery_blank_ms;
    uint16_t gap_release_ms;
} fsr_sentinel_v2_params_t;

typedef struct {
    fsr_sentinel_v2_params_t signal;
    uint16_t motion_threshold;
    uint16_t motion_touch_delta;
    uint16_t motion_hold_ms;
    uint16_t motion_release_extra_ms;
} fsr_sentinel_v3_params_t;

typedef struct {
    uint16_t median_values[3];
    uint8_t median_count;
    uint8_t median_next;
    fsr_sentinel_state_t state;
    int32_t center_q8;
    int32_t peak_q8;
    int16_t touch_anchor;
    uint16_t prior_filtered;
    uint16_t touch_evidence_ms;
    uint16_t release_evidence_ms;
    uint32_t prior_t_ms;
    uint32_t recovery_started_ms;
    uint32_t motion_until_ms;
    bool initialized;
    bool motion_seen;
} fsr_sentinel_successor_t;

typedef struct {
    fsr_sentinel_algorithm_id_t algorithm;
    union {
        fsr_sentinel_t v1;
        fsr_sentinel_successor_t successor;
        fsr_atlas_phase_state_t atlas_phase;
    } detector;
} fsr_sentinel_runtime_t;

extern fsr_sentinel_v2_params_t g_fsr_sentinel_v2_params;
extern fsr_sentinel_v3_params_t g_fsr_sentinel_v3_params;

const fsr_sentinel_algorithm_meta_t *fsr_sentinel_runtime_algorithm_meta(
    uint8_t index);
uint8_t fsr_sentinel_runtime_active_algorithm(void);
bool fsr_sentinel_runtime_set_algorithm(uint8_t algorithm);
uint16_t fsr_sentinel_runtime_generation(void);
uint16_t fsr_sentinel_runtime_interval_ms(
    const fsr_sentinel_runtime_t *runtime, uint16_t legacy_interval_ms);

uint8_t fsr_sentinel_runtime_param_count(uint8_t algorithm);
const fsr_sentinel_param_meta_t *fsr_sentinel_runtime_param_meta(
    uint8_t algorithm, uint8_t index);
bool fsr_sentinel_runtime_param_get(uint8_t algorithm, uint8_t index,
                                    int16_t *value);
bool fsr_sentinel_runtime_param_set(uint8_t algorithm, uint8_t index,
                                    int16_t value);
bool fsr_sentinel_runtime_params_reset(uint8_t algorithm);

void fsr_sentinel_runtime_init(fsr_sentinel_runtime_t *runtime);
fsr_sentinel_output_t fsr_sentinel_runtime_step(
    fsr_sentinel_runtime_t *runtime, uint16_t reading, uint32_t t_ms,
    uint32_t scan, int16_t motion_x, int16_t motion_y);
int32_t fsr_sentinel_runtime_center(const fsr_sentinel_runtime_t *runtime);
uint16_t fsr_sentinel_runtime_filtered(const fsr_sentinel_runtime_t *runtime);
uint16_t fsr_sentinel_runtime_touch_evidence(
    const fsr_sentinel_runtime_t *runtime);
uint16_t fsr_sentinel_runtime_release_evidence(
    const fsr_sentinel_runtime_t *runtime);
