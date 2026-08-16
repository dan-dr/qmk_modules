// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../fsr_atlas_phase.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 1 + FSR_ATLAS_PHASE_PARAM_COUNT) return 2;
    for (uint8_t index = 0; index < FSR_ATLAS_PHASE_PARAM_COUNT; ++index) {
        long value = strtol(argv[index + 1], NULL, 10);
        if (value < INT16_MIN || value > INT16_MAX ||
            !fsr_atlas_phase_params_set(index, (int16_t)value)) {
            return 2;
        }
    }
    fsr_atlas_phase_state_t state;
    fsr_atlas_phase_reset(&state);
    unsigned reading;
    unsigned t_ms;
    int motion_x;
    int motion_y;
    while (scanf("%u,%u,%d,%d", &reading, &t_ms, &motion_x, &motion_y) == 4) {
        fsr_atlas_phase_output_t output = fsr_atlas_phase_step(
            &state, (uint16_t)reading, (uint32_t)t_ms,
            (int16_t)motion_x, (int16_t)motion_y);
        printf("%u\n", output.touched ? 1U : 0U);
    }
    return 0;
}
