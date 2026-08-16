// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../fsr_sentinel_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    unsigned algorithm = (unsigned)strtoul(argv[1], NULL, 10);
    if (!fsr_sentinel_runtime_set_algorithm((uint8_t)algorithm)) {
        return 2;
    }
    fsr_sentinel_runtime_t runtime;
    fsr_sentinel_runtime_init(&runtime);

    unsigned reading;
    unsigned t_ms;
    unsigned scan;
    int motion_x;
    int motion_y;
    while (scanf("%u,%u,%u,%d,%d", &reading, &t_ms, &scan, &motion_x,
                 &motion_y) == 5) {
        fsr_sentinel_output_t output = fsr_sentinel_runtime_step(
            &runtime, (uint16_t)reading, (uint32_t)t_ms, (uint32_t)scan,
            (int16_t)motion_x, (int16_t)motion_y);
        printf("%u,%u,%ld,%u,%u\n", output.touched ? 1U : 0U,
               (unsigned)output.state,
               (long)fsr_sentinel_runtime_center(&runtime),
               (unsigned)fsr_sentinel_runtime_touch_evidence(&runtime),
               (unsigned)fsr_sentinel_runtime_release_evidence(&runtime));
    }
    return 0;
}
