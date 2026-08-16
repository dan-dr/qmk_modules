// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../fsr_sentinel.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static fsr_sentinel_output_t step(fsr_sentinel_t *sentinel,
                                  uint16_t reading,
                                  uint32_t t_ms,
                                  uint32_t scan) {
    return fsr_sentinel_step(sentinel, reading, t_ms, scan);
}

static void test_seed_touch_release_and_recovery(void) {
    fsr_sentinel_t sentinel;
    fsr_sentinel_init(&sentinel);

    fsr_sentinel_output_t output = step(&sentinel, 3000, 0, 1);
    assert(!output.touched);
    assert(output.state == FSR_SENTINEL_IDLE);
    assert(output.reason == FSR_SENTINEL_REASON_SEED);

    output = step(&sentinel, 3120, 20, 2);
    assert(output.touched);
    assert(output.state == FSR_SENTINEL_TOUCHED);
    assert(output.reason == FSR_SENTINEL_REASON_POSITIVE_CUSUM);

    output = step(&sentinel, 3000, 40, 3);
    assert(!output.touched);
    assert(output.state == FSR_SENTINEL_RECOVERY);
    assert(output.reason == FSR_SENTINEL_REASON_NEGATIVE_CUSUM);

    output = step(&sentinel, 3000, 99, 4);
    assert(output.state == FSR_SENTINEL_RECOVERY);
    output = step(&sentinel, 3000, 100, 5);
    assert(output.state == FSR_SENTINEL_IDLE);
    assert(output.reason == FSR_SENTINEL_REASON_RECOVERY_COMPLETE);
}

static void test_scan_gap_clears_accumulated_evidence(void) {
    fsr_sentinel_t sentinel;
    fsr_sentinel_init(&sentinel);
    step(&sentinel, 3000, 0, 1);
    step(&sentinel, 3000, 20, 2);
    step(&sentinel, 3100, 40, 3);

    fsr_sentinel_output_t output = step(&sentinel, 3100, 60, 4);
    assert(!output.touched);
    assert(sentinel.positive_score == 80);

    output = step(&sentinel, 3100, 80, 6);
    assert(!output.touched);
    assert(sentinel.positive_score == 50);
}

static void test_timer_and_scan_wrap_are_unsigned(void) {
    fsr_sentinel_t sentinel;
    fsr_sentinel_init(&sentinel);
    step(&sentinel, 3000, UINT32_MAX - 19U, UINT32_MAX);
    fsr_sentinel_output_t output = step(&sentinel, 3120, 0, 0);
    assert(output.touched);
    assert(output.reason == FSR_SENTINEL_REASON_POSITIVE_CUSUM);
}

static void test_frozen_parameters_and_state_budget(void) {
    assert(FSR_SENTINEL_CENTER_RATE_PER_20_MS == 30U);
    assert(FSR_SENTINEL_RECOVERY_BLANK_MS == 60U);
    assert(FSR_SENTINEL_RELEASE_DRIFT == 0);
    assert(FSR_SENTINEL_RELEASE_SCORE == 70U);
    assert(FSR_SENTINEL_SCORE_LEAK_PER_20_MS == 5);
    assert(FSR_SENTINEL_TOUCH_DRIFT == 15);
    assert(FSR_SENTINEL_TOUCH_SCORE == 100U);
    assert(sizeof(fsr_sentinel_t) <= FSR_SENTINEL_STATE_BUDGET_BYTES);
    fsr_sentinel_params_reset();
    assert(g_fsr_sentinel_params.center_rate_per_20_ms == 30U);
    assert(g_fsr_sentinel_params.recovery_blank_ms == 60U);
    assert(g_fsr_sentinel_params.release_drift == 0);
    assert(g_fsr_sentinel_params.release_score == 70U);
    assert(g_fsr_sentinel_params.score_leak_per_20_ms == 5);
    assert(g_fsr_sentinel_params.touch_drift == 15);
    assert(g_fsr_sentinel_params.touch_score == 100U);
    assert(FSR_SENTINEL_PARAM_COUNT == 7);
    assert(fsr_sentinel_param_meta(0) != NULL);
    assert(fsr_sentinel_param_set(FSR_SENTINEL_PARAM_TOUCH_SCORE, 80));
    int16_t value = 0;
    assert(fsr_sentinel_param_get(FSR_SENTINEL_PARAM_TOUCH_SCORE, &value));
    assert(value == 80);
    fsr_sentinel_params_reset();
}

int main(void) {
    test_seed_touch_release_and_recovery();
    test_scan_gap_clears_accumulated_evidence();
    test_timer_and_scan_wrap_are_unsigned();
    test_frozen_parameters_and_state_budget();
    puts("fsr_sentinel_test: ok");
    return 0;
}
