// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* FSR Sentinel v1, frozen evidence identifier: run7-provisional-v1. */
#ifndef FSR_SENTINEL_CENTER_RATE_PER_20_MS
#    define FSR_SENTINEL_CENTER_RATE_PER_20_MS 30U
#endif

#ifndef FSR_SENTINEL_RECOVERY_BLANK_MS
#    define FSR_SENTINEL_RECOVERY_BLANK_MS 60U
#endif

#ifndef FSR_SENTINEL_RELEASE_DRIFT
#    define FSR_SENTINEL_RELEASE_DRIFT 0
#endif

#ifndef FSR_SENTINEL_RELEASE_SCORE
#    define FSR_SENTINEL_RELEASE_SCORE 70U
#endif

#ifndef FSR_SENTINEL_SCORE_LEAK_PER_20_MS
#    define FSR_SENTINEL_SCORE_LEAK_PER_20_MS 5
#endif

#ifndef FSR_SENTINEL_TOUCH_DRIFT
#    define FSR_SENTINEL_TOUCH_DRIFT 15
#endif

#ifndef FSR_SENTINEL_TOUCH_SCORE
#    define FSR_SENTINEL_TOUCH_SCORE 100U
#endif

#define FSR_SENTINEL_STATE_BUDGET_BYTES 56U

/* Bump when adding/removing/reordering runtime parameters (WebHID schema). */
#define FSR_SENTINEL_PARAMS_VERSION 1U

typedef enum {
    FSR_SENTINEL_PARAM_CENTER_RATE = 0,
    FSR_SENTINEL_PARAM_RECOVERY_BLANK_MS,
    FSR_SENTINEL_PARAM_RELEASE_DRIFT,
    FSR_SENTINEL_PARAM_RELEASE_SCORE,
    FSR_SENTINEL_PARAM_SCORE_LEAK,
    FSR_SENTINEL_PARAM_TOUCH_DRIFT,
    FSR_SENTINEL_PARAM_TOUCH_SCORE,
    FSR_SENTINEL_PARAM_COUNT,
} fsr_sentinel_param_id_t;

typedef struct {
    uint16_t center_rate_per_20_ms;
    uint16_t recovery_blank_ms;
    int16_t release_drift;
    uint16_t release_score;
    int16_t score_leak_per_20_ms;
    int16_t touch_drift;
    uint16_t touch_score;
} fsr_sentinel_params_t;

typedef struct {
    uint8_t id;
    const char *key;
    const char *label;
    int16_t min;
    int16_t max;
    int16_t default_value;
} fsr_sentinel_param_meta_t;

typedef enum {
    FSR_SENTINEL_IDLE = 0,
    FSR_SENTINEL_TOUCHED,
    FSR_SENTINEL_RECOVERY,
} fsr_sentinel_state_t;

typedef enum {
    FSR_SENTINEL_REASON_NONE = 0,
    FSR_SENTINEL_REASON_SEED,
    FSR_SENTINEL_REASON_POSITIVE_CUSUM,
    FSR_SENTINEL_REASON_NEGATIVE_CUSUM,
    FSR_SENTINEL_REASON_RECOVERY_COMPLETE,
    FSR_SENTINEL_REASON_ENVELOPE_TOUCH,
    FSR_SENTINEL_REASON_ENVELOPE_RELEASE,
    FSR_SENTINEL_REASON_SCAN_GAP,
} fsr_sentinel_reason_t;

typedef struct {
    uint16_t median_values[3];
    uint8_t median_count;
    uint8_t median_next;
    fsr_sentinel_state_t state;
    int32_t center_q8;
    uint16_t positive_score;
    uint16_t negative_score;
    uint16_t prior_filtered;
    uint32_t prior_t_ms;
    uint32_t recovery_started_ms;
    uint32_t prior_scan;
    bool initialized;
} fsr_sentinel_t;

typedef struct {
    bool touched;
    fsr_sentinel_state_t state;
    fsr_sentinel_reason_t reason;
} fsr_sentinel_output_t;

extern fsr_sentinel_params_t g_fsr_sentinel_params;

void fsr_sentinel_params_reset(void);
const fsr_sentinel_param_meta_t *fsr_sentinel_param_meta(uint8_t id);
bool fsr_sentinel_param_get(uint8_t id, int16_t *value);
bool fsr_sentinel_param_set(uint8_t id, int16_t value);

void fsr_sentinel_init(fsr_sentinel_t *sentinel);
fsr_sentinel_output_t fsr_sentinel_step(fsr_sentinel_t *sentinel,
                                        uint16_t reading,
                                        uint32_t t_ms,
                                        uint32_t scan);
const char *fsr_sentinel_state_name(fsr_sentinel_state_t state);
const char *fsr_sentinel_reason_name(fsr_sentinel_reason_t reason);
