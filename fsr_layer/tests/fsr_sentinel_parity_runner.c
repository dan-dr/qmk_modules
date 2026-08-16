// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../fsr_sentinel.h"
#include "fsr_detector_reference.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static bool states_match(const fsr_sentinel_t *sentinel,
                         const run7_fsr_detector_t *reference) {
    for (uint8_t index = 0; index < 3U; index++) {
        if (sentinel->median_values[index] !=
            reference->median_values[index]) {
            return false;
        }
    }
    return sentinel->median_count == reference->median_count &&
           sentinel->median_next == reference->median_next &&
           (unsigned)sentinel->state == (unsigned)reference->state &&
           sentinel->center_q8 == reference->center_q8 &&
           sentinel->positive_score == reference->positive_score &&
           sentinel->negative_score == reference->negative_score &&
           sentinel->prior_filtered == reference->prior_filtered &&
           sentinel->prior_t_ms == reference->prior_t_ms &&
           sentinel->recovery_started_ms ==
               reference->recovery_started_ms &&
           sentinel->prior_scan == reference->prior_scan &&
           sentinel->initialized == reference->initialized;
}

int main(void) {
    fsr_sentinel_t sentinel;
    run7_fsr_detector_t reference;
    fsr_sentinel_init(&sentinel);
    run7_fsr_detector_init(&reference);

    unsigned reading;
    unsigned t_ms;
    unsigned scan;
    unsigned rows = 0;
    while (scanf("%u,%u,%u", &reading, &t_ms, &scan) == 3) {
        rows++;
        fsr_sentinel_output_t sentinel_output = fsr_sentinel_step(
            &sentinel, (uint16_t)reading, (uint32_t)t_ms, (uint32_t)scan);
        run7_fsr_output_t reference_output = run7_fsr_detector_step(
            &reference, (uint16_t)reading, (uint32_t)t_ms, (uint32_t)scan);
        if (sentinel_output.touched != reference_output.touched ||
            (unsigned)sentinel_output.state !=
                (unsigned)reference_output.state ||
            (unsigned)sentinel_output.reason !=
                (unsigned)reference_output.reason ||
            !states_match(&sentinel, &reference)) {
            fprintf(stderr,
                    "FSR Sentinel parity mismatch at row %u "
                    "reading=%u t_ms=%u scan=%u\n",
                    rows, reading, t_ms, scan);
            return 1;
        }
    }
    printf("fsr_sentinel_parity: %u rows ok\n", rows);
    return rows == 0U;
}
