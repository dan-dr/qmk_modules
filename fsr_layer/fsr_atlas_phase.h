// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FSR_ATLAS_PHASE_PARAM_COUNT 17U
#define FSR_ATLAS_PHASE_HISTORY_COUNT 130U
#define FSR_ATLAS_PHASE_FRONTEND_COUNT 17U

typedef enum {
    FSR_ATLAS_FRONTEND_RAW = 0,
    FSR_ATLAS_FRONTEND_MEAN = 1,
    FSR_ATLAS_FRONTEND_MEDIAN = 2,
    FSR_ATLAS_FRONTEND_SLEW_LIMITED = 3,
    FSR_ATLAS_FRONTEND_EXPONENTIAL = 4,
} fsr_atlas_frontend_t;

typedef enum {
    FSR_ATLAS_PARAM_FRONTEND = 0,
    FSR_ATLAS_PARAM_FRONTEND_PARAM,
    FSR_ATLAS_PARAM_CADENCE_MS,
    FSR_ATLAS_PARAM_GAP_MS,
    FSR_ATLAS_PARAM_MOTION_MODE,
    FSR_ATLAS_PARAM_MOTION_WINDOW_MS,
    FSR_ATLAS_PARAM_MOTION_FORCE_FLOOR,
    FSR_ATLAS_PARAM_MOTION_THRESHOLD,
    FSR_ATLAS_PARAM_BASELINE_TAU_MS,
    FSR_ATLAS_PARAM_EXCURSION_WINDOW_MS,
    FSR_ATLAS_PARAM_ONSET_RISE,
    FSR_ATLAS_PARAM_RELEASE_FALL,
    FSR_ATLAS_PARAM_ONSET_RESIDUAL,
    FSR_ATLAS_PARAM_RELEASE_RESIDUAL,
    FSR_ATLAS_PARAM_ONSET_DWELL_MS,
    FSR_ATLAS_PARAM_RELEASE_DWELL_MS,
    FSR_ATLAS_PARAM_RETOUCH_RISE,
} fsr_atlas_phase_param_id_t;

/* All fields are live-editable signed 16-bit protocol parameters. */
typedef struct {
    int16_t frontend;
    int16_t frontend_param;
    int16_t cadence_ms;
    int16_t gap_ms;
    int16_t motion_mode; /* bit0 enter, bit1 hold, bit2 no press required */
    int16_t motion_window_ms;
    int16_t motion_force_floor;
    int16_t motion_threshold;
    int16_t p0; /* unloaded baseline EWMA tau, ms */
    int16_t p1; /* causal excursion window, ms */
    int16_t p2; /* onset rise from recent floor */
    int16_t p3; /* release fall from recent peak */
    int16_t p4; /* onset residual from unloaded center */
    int16_t p5; /* release residual from unloaded center */
    int16_t p6; /* onset dwell, ms */
    int16_t p7; /* release dwell, ms */
    int16_t p8; /* force-plus-motion retouch rise */
} fsr_atlas_phase_params_t;

typedef struct {
    bool touched;
    bool changed;
    bool evaluated;
    bool gap_reset;
    uint16_t filtered;
} fsr_atlas_phase_output_t;

typedef struct {
    bool initialized;
    bool touched;
    bool pending_value;
    bool pending_valid;
    bool quarantine_active;
    bool family_saw_touched;
    uint32_t pending_since;
    uint32_t last_sample_ms;
    uint32_t last_eval_ms;
    uint32_t last_motion_ms;
    uint32_t quarantine_since;
    uint64_t count;
    uint32_t last_cadence_ms;
    int32_t front_value_q8;
    int32_t gate_base_q8;
    int32_t base_q8;
    int32_t front_sum;
    int32_t release_floor;
    int32_t on_evidence;
    int32_t release_evidence;
    uint16_t filtered;
    int16_t front[FSR_ATLAS_PHASE_FRONTEND_COUNT];
    uint8_t front_count;
    uint8_t front_cursor;
    int16_t history[FSR_ATLAS_PHASE_HISTORY_COUNT];
} fsr_atlas_phase_state_t;

extern fsr_atlas_phase_params_t g_fsr_atlas_phase_params;

void fsr_atlas_phase_params_reset(void);
bool fsr_atlas_phase_params_get(uint8_t index, int16_t *value);
bool fsr_atlas_phase_params_set(uint8_t index, int16_t value);
void fsr_atlas_phase_reset(fsr_atlas_phase_state_t *state);
void fsr_atlas_phase_init(fsr_atlas_phase_state_t *state, uint16_t reading,
                          uint32_t t_ms);
fsr_atlas_phase_output_t fsr_atlas_phase_step(
    fsr_atlas_phase_state_t *state, uint16_t reading, uint32_t t_ms,
    int16_t motion_x, int16_t motion_y);

int32_t fsr_atlas_phase_center(const fsr_atlas_phase_state_t *state);
uint16_t fsr_atlas_phase_filtered(const fsr_atlas_phase_state_t *state);
int32_t fsr_atlas_phase_touch_evidence(const fsr_atlas_phase_state_t *state);
int32_t fsr_atlas_phase_release_evidence(const fsr_atlas_phase_state_t *state);
uint32_t fsr_atlas_phase_cadence_ms(const fsr_atlas_phase_state_t *state);
uint16_t fsr_atlas_phase_interval_ms(void);
