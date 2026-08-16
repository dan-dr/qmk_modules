// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fsr_sentinel_runtime.h"

#include "fsr_sentinel_nvm.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

enum {
    V2_PARAM_CENTER_RATE = 0,
    V2_PARAM_TOUCH_DELTA,
    V2_PARAM_TOUCH_CONFIRM_MS,
    V2_PARAM_PEAK_DECAY,
    V2_PARAM_RELEASE_DROP,
    V2_PARAM_RELEASE_FLOOR_MARGIN,
    V2_PARAM_RELEASE_CONFIRM_MS,
    V2_PARAM_RECOVERY_BLANK_MS,
    V2_PARAM_GAP_RELEASE_MS,
    V2_PARAM_COUNT,
};

enum {
    V3_PARAM_MOTION_THRESHOLD = V2_PARAM_COUNT,
    V3_PARAM_MOTION_TOUCH_DELTA,
    V3_PARAM_MOTION_HOLD_MS,
    V3_PARAM_MOTION_RELEASE_EXTRA_MS,
    V3_PARAM_COUNT,
};

_Static_assert(sizeof(fsr_sentinel_runtime_t) <=
                   FSR_SENTINEL_RUNTIME_STATE_BUDGET_BYTES,
               "FSR Sentinel runtime state exceeds its firmware budget");
_Static_assert(FSR_ATLAS_PHASE_PARAM_COUNT == 17U,
               "FSR Atlas registry must expose every editable parameter");

fsr_sentinel_v2_params_t g_fsr_sentinel_v2_params = {
    .center_rate = 20,
    .touch_delta = 90,
    .touch_confirm_ms = 80,
    .peak_decay = 40,
    .release_drop = 80,
    .release_floor_margin = 90,
    .release_confirm_ms = 40,
    .recovery_blank_ms = 100,
    .gap_release_ms = 200,
};

fsr_sentinel_v3_params_t g_fsr_sentinel_v3_params = {
    .signal = {
        .center_rate = 30,
        .touch_delta = 70,
        .touch_confirm_ms = 80,
        .peak_decay = 5,
        .release_drop = 220,
        .release_floor_margin = 30,
        .release_confirm_ms = 40,
        .recovery_blank_ms = 100,
        .gap_release_ms = 200,
    },
    .motion_threshold = 6,
    .motion_touch_delta = 30,
    .motion_hold_ms = 20,
    .motion_release_extra_ms = 40,
};

static uint8_t active_algorithm = FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1;
static uint16_t runtime_generation;

static const fsr_sentinel_algorithm_meta_t algorithms[] = {
    {FSR_SENTINEL_ALGORITHM_V1, "v1_dual_cusum", FSR_SENTINEL_PARAM_COUNT},
    {FSR_SENTINEL_ALGORITHM_V2, "v2_signal_envelope", V2_PARAM_COUNT},
    {FSR_SENTINEL_ALGORITHM_V3, "v3_motion_envelope", V3_PARAM_COUNT},
    {FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1, "atlas_phase_v1",
     FSR_ATLAS_PHASE_PARAM_COUNT},
};

static const fsr_sentinel_param_meta_t v2_meta[V2_PARAM_COUNT] = {
    {V2_PARAM_CENTER_RATE, "center_rate", "Center rate", 1, 4095, 20},
    {V2_PARAM_TOUCH_DELTA, "touch_delta", "Touch delta", 1, 4095, 90},
    {V2_PARAM_TOUCH_CONFIRM_MS, "touch_confirm_ms", "Touch confirm", 1, 5000, 80},
    {V2_PARAM_PEAK_DECAY, "peak_decay", "Peak decay", 0, 4095, 40},
    {V2_PARAM_RELEASE_DROP, "release_drop", "Release drop", 1, 4095, 80},
    {V2_PARAM_RELEASE_FLOOR_MARGIN, "release_floor", "Release floor", -4095, 4095, 90},
    {V2_PARAM_RELEASE_CONFIRM_MS, "release_confirm_ms", "Release confirm", 1, 5000, 40},
    {V2_PARAM_RECOVERY_BLANK_MS, "recovery_blank_ms", "Recovery blank", 1, 5000, 100},
    {V2_PARAM_GAP_RELEASE_MS, "gap_release_ms", "Gap release", 20, 5000, 200},
};

static const fsr_sentinel_param_meta_t v3_meta[V3_PARAM_COUNT] = {
    {V2_PARAM_CENTER_RATE, "center_rate", "Center rate", 1, 4095, 30},
    {V2_PARAM_TOUCH_DELTA, "touch_delta", "Touch delta", 1, 4095, 70},
    {V2_PARAM_TOUCH_CONFIRM_MS, "touch_confirm_ms", "Touch confirm", 1, 5000, 80},
    {V2_PARAM_PEAK_DECAY, "peak_decay", "Peak decay", 0, 4095, 5},
    {V2_PARAM_RELEASE_DROP, "release_drop", "Release drop", 1, 4095, 220},
    {V2_PARAM_RELEASE_FLOOR_MARGIN, "release_floor", "Release floor", -4095, 4095, 30},
    {V2_PARAM_RELEASE_CONFIRM_MS, "release_confirm_ms", "Release confirm", 1, 5000, 40},
    {V2_PARAM_RECOVERY_BLANK_MS, "recovery_blank_ms", "Recovery blank", 1, 5000, 100},
    {V2_PARAM_GAP_RELEASE_MS, "gap_release_ms", "Gap release", 20, 5000, 200},
    {V3_PARAM_MOTION_THRESHOLD, "motion_threshold", "Motion threshold", 1, 4095, 6},
    {V3_PARAM_MOTION_TOUCH_DELTA, "motion_touch_delta", "Motion touch delta", 1, 4095, 30},
    {V3_PARAM_MOTION_HOLD_MS, "motion_hold_ms", "Motion hold", 0, 5000, 20},
    {V3_PARAM_MOTION_RELEASE_EXTRA_MS, "motion_release_ms", "Motion release extra", 0, 5000, 40},
};

static const fsr_sentinel_param_meta_t
    atlas_meta[FSR_ATLAS_PHASE_PARAM_COUNT] = {
        {FSR_ATLAS_PARAM_FRONTEND, "frontend", "Signal front end", 0, 4, 0},
        {FSR_ATLAS_PARAM_FRONTEND_PARAM, "frontend_param", "Front-end parameter", 1, 1000, 1},
        {FSR_ATLAS_PARAM_CADENCE_MS, "cadence_ms", "Detector cadence", 1, 20, 1},
        {FSR_ATLAS_PARAM_GAP_MS, "gap_ms", "Gap reset", 20, 1000, 100},
        {FSR_ATLAS_PARAM_MOTION_MODE, "motion_mode", "Motion mode", 0, 7, 1},
        {FSR_ATLAS_PARAM_MOTION_WINDOW_MS, "motion_window_ms", "Motion window", 0, 500, 5},
        {FSR_ATLAS_PARAM_MOTION_FORCE_FLOOR, "motion_force_floor", "Motion force floor", 0, 500, 10},
        {FSR_ATLAS_PARAM_MOTION_THRESHOLD, "motion_threshold", "Motion threshold", 0, 1000, 5},
        {FSR_ATLAS_PARAM_BASELINE_TAU_MS, "baseline_tau_ms", "Baseline time constant", 1, 10000, 1500},
        {FSR_ATLAS_PARAM_EXCURSION_WINDOW_MS, "excursion_ms", "Excursion window", 1, 1280, 8},
        {FSR_ATLAS_PARAM_ONSET_RISE, "onset_rise", "Onset rise", 1, 1000, 45},
        {FSR_ATLAS_PARAM_RELEASE_FALL, "release_fall", "Release fall", 1, 1000, 40},
        {FSR_ATLAS_PARAM_ONSET_RESIDUAL, "onset_residual", "Onset residual", 1, 1500, 70},
        {FSR_ATLAS_PARAM_RELEASE_RESIDUAL, "release_residual", "Release residual", -500, 1499, 40},
        {FSR_ATLAS_PARAM_ONSET_DWELL_MS, "onset_dwell_ms", "Onset dwell", 0, 1000, 0},
        {FSR_ATLAS_PARAM_RELEASE_DWELL_MS, "release_dwell_ms", "Release dwell", 0, 1000, 5},
        {FSR_ATLAS_PARAM_RETOUCH_RISE, "retouch_rise", "Motion retouch rise", 1, 1000, 40},
};

const fsr_sentinel_algorithm_meta_t *fsr_sentinel_runtime_algorithm_meta(
    uint8_t index) {
    return index < FSR_SENTINEL_ALGORITHM_COUNT ? &algorithms[index] : NULL;
}

uint8_t fsr_sentinel_runtime_active_algorithm(void) {
    return active_algorithm;
}

bool fsr_sentinel_runtime_set_algorithm(uint8_t algorithm) {
    if (algorithm < FSR_SENTINEL_ALGORITHM_V1 ||
        algorithm > FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        return false;
    }
    if (active_algorithm != algorithm) {
        active_algorithm = algorithm;
        runtime_generation++;
        fsr_sentinel_nvm_mark_dirty();
    }
    return true;
}

uint16_t fsr_sentinel_runtime_generation(void) {
    return runtime_generation;
}

uint8_t fsr_sentinel_runtime_param_count(uint8_t algorithm) {
    if (algorithm == FSR_SENTINEL_ALGORITHM_V1) {
        return FSR_SENTINEL_PARAM_COUNT;
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_V2) {
        return V2_PARAM_COUNT;
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_V3) {
        return V3_PARAM_COUNT;
    }
    return algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1
               ? FSR_ATLAS_PHASE_PARAM_COUNT
               : 0;
}

const fsr_sentinel_param_meta_t *fsr_sentinel_runtime_param_meta(
    uint8_t algorithm, uint8_t index) {
    if (algorithm == FSR_SENTINEL_ALGORITHM_V1) {
        return fsr_sentinel_param_meta(index);
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_V2) {
        return index < V2_PARAM_COUNT ? &v2_meta[index] : NULL;
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_V3) {
        return index < V3_PARAM_COUNT ? &v3_meta[index] : NULL;
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        return index < FSR_ATLAS_PHASE_PARAM_COUNT ? &atlas_meta[index] : NULL;
    }
    return NULL;
}

static bool successor_get(const fsr_sentinel_v2_params_t *params,
                          uint8_t index, int16_t *value) {
    switch (index) {
        case V2_PARAM_CENTER_RATE: *value = (int16_t)params->center_rate; break;
        case V2_PARAM_TOUCH_DELTA: *value = (int16_t)params->touch_delta; break;
        case V2_PARAM_TOUCH_CONFIRM_MS: *value = (int16_t)params->touch_confirm_ms; break;
        case V2_PARAM_PEAK_DECAY: *value = (int16_t)params->peak_decay; break;
        case V2_PARAM_RELEASE_DROP: *value = (int16_t)params->release_drop; break;
        case V2_PARAM_RELEASE_FLOOR_MARGIN: *value = params->release_floor_margin; break;
        case V2_PARAM_RELEASE_CONFIRM_MS: *value = (int16_t)params->release_confirm_ms; break;
        case V2_PARAM_RECOVERY_BLANK_MS: *value = (int16_t)params->recovery_blank_ms; break;
        case V2_PARAM_GAP_RELEASE_MS: *value = (int16_t)params->gap_release_ms; break;
        default: return false;
    }
    return true;
}

bool fsr_sentinel_runtime_param_get(uint8_t algorithm, uint8_t index,
                                    int16_t *value) {
    if (value == NULL) {
        return false;
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_V1) {
        return fsr_sentinel_param_get(index, value);
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_V2) {
        return successor_get(&g_fsr_sentinel_v2_params, index, value);
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_V3) {
        if (successor_get(&g_fsr_sentinel_v3_params.signal, index, value)) {
            return true;
        }
        switch (index) {
            case V3_PARAM_MOTION_THRESHOLD: *value = (int16_t)g_fsr_sentinel_v3_params.motion_threshold; return true;
            case V3_PARAM_MOTION_TOUCH_DELTA: *value = (int16_t)g_fsr_sentinel_v3_params.motion_touch_delta; return true;
            case V3_PARAM_MOTION_HOLD_MS: *value = (int16_t)g_fsr_sentinel_v3_params.motion_hold_ms; return true;
            case V3_PARAM_MOTION_RELEASE_EXTRA_MS: *value = (int16_t)g_fsr_sentinel_v3_params.motion_release_extra_ms; return true;
            default: return false;
        }
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        return fsr_atlas_phase_params_get(index, value);
    }
    return false;
}

static bool successor_set(fsr_sentinel_v2_params_t *params, uint8_t index,
                          int16_t value) {
    switch (index) {
        case V2_PARAM_CENTER_RATE: params->center_rate = (uint16_t)value; break;
        case V2_PARAM_TOUCH_DELTA: params->touch_delta = (uint16_t)value; break;
        case V2_PARAM_TOUCH_CONFIRM_MS: params->touch_confirm_ms = (uint16_t)value; break;
        case V2_PARAM_PEAK_DECAY: params->peak_decay = (uint16_t)value; break;
        case V2_PARAM_RELEASE_DROP: params->release_drop = (uint16_t)value; break;
        case V2_PARAM_RELEASE_FLOOR_MARGIN: params->release_floor_margin = value; break;
        case V2_PARAM_RELEASE_CONFIRM_MS: params->release_confirm_ms = (uint16_t)value; break;
        case V2_PARAM_RECOVERY_BLANK_MS: params->recovery_blank_ms = (uint16_t)value; break;
        case V2_PARAM_GAP_RELEASE_MS: params->gap_release_ms = (uint16_t)value; break;
        default: return false;
    }
    return true;
}

bool fsr_sentinel_runtime_param_set(uint8_t algorithm, uint8_t index,
                                    int16_t value) {
    const fsr_sentinel_param_meta_t *meta =
        fsr_sentinel_runtime_param_meta(algorithm, index);
    if (meta == NULL || value < meta->min || value > meta->max) {
        return false;
    }
    bool changed = false;
    int16_t prior = 0;
    fsr_sentinel_runtime_param_get(algorithm, index, &prior);
    if (algorithm == FSR_SENTINEL_ALGORITHM_V1) {
        changed = fsr_sentinel_param_set(index, value);
    } else if (algorithm == FSR_SENTINEL_ALGORITHM_V2) {
        changed = successor_set(&g_fsr_sentinel_v2_params, index, value);
    } else if (algorithm == FSR_SENTINEL_ALGORITHM_V3) {
        changed = successor_set(&g_fsr_sentinel_v3_params.signal, index, value);
        if (!changed) {
            switch (index) {
                case V3_PARAM_MOTION_THRESHOLD: g_fsr_sentinel_v3_params.motion_threshold = (uint16_t)value; changed = true; break;
                case V3_PARAM_MOTION_TOUCH_DELTA: g_fsr_sentinel_v3_params.motion_touch_delta = (uint16_t)value; changed = true; break;
                case V3_PARAM_MOTION_HOLD_MS: g_fsr_sentinel_v3_params.motion_hold_ms = (uint16_t)value; changed = true; break;
                case V3_PARAM_MOTION_RELEASE_EXTRA_MS: g_fsr_sentinel_v3_params.motion_release_extra_ms = (uint16_t)value; changed = true; break;
                default: break;
            }
        }
    } else if (algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        changed = fsr_atlas_phase_params_set(index, value);
    }
    if (changed && prior != value) {
        runtime_generation++;
        fsr_sentinel_nvm_mark_dirty();
    }
    return changed;
}

bool fsr_sentinel_runtime_params_reset(uint8_t algorithm) {
    uint8_t count = fsr_sentinel_runtime_param_count(algorithm);
    if (count == 0) {
        return false;
    }
    if (algorithm == FSR_SENTINEL_ALGORITHM_V1) {
        fsr_sentinel_params_reset();
    } else if (algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        fsr_atlas_phase_params_reset();
    } else {
        for (uint8_t index = 0; index < count; index++) {
            const fsr_sentinel_param_meta_t *meta =
                fsr_sentinel_runtime_param_meta(algorithm, index);
            if (algorithm == FSR_SENTINEL_ALGORITHM_V2) {
                successor_set(&g_fsr_sentinel_v2_params, index,
                              meta->default_value);
            } else if (algorithm == FSR_SENTINEL_ALGORITHM_V3) {
                if (!successor_set(&g_fsr_sentinel_v3_params.signal, index,
                                   meta->default_value)) {
                    switch (index) {
                        case V3_PARAM_MOTION_THRESHOLD:
                            g_fsr_sentinel_v3_params.motion_threshold =
                                (uint16_t)meta->default_value;
                            break;
                        case V3_PARAM_MOTION_TOUCH_DELTA:
                            g_fsr_sentinel_v3_params.motion_touch_delta =
                                (uint16_t)meta->default_value;
                            break;
                        case V3_PARAM_MOTION_HOLD_MS:
                            g_fsr_sentinel_v3_params.motion_hold_ms =
                                (uint16_t)meta->default_value;
                            break;
                        case V3_PARAM_MOTION_RELEASE_EXTRA_MS:
                            g_fsr_sentinel_v3_params.motion_release_extra_ms =
                                (uint16_t)meta->default_value;
                            break;
                        default: break;
                    }
                }
            }
        }
    }
    runtime_generation++;
    fsr_sentinel_nvm_mark_dirty();
    return true;
}

static uint16_t median_step(fsr_sentinel_successor_t *state,
                            uint16_t reading) {
    state->median_values[state->median_next] = reading;
    state->median_next = (uint8_t)((state->median_next + 1U) % 3U);
    if (state->median_count < 3U) {
        state->median_count++;
    }
    uint16_t values[3];
    memcpy(values, state->median_values,
           (size_t)state->median_count * sizeof(values[0]));
    for (uint8_t left = 1; left < state->median_count; left++) {
        uint16_t value = values[left];
        uint8_t right = left;
        while (right > 0U && values[right - 1U] > value) {
            values[right] = values[right - 1U];
            right--;
        }
        values[right] = value;
    }
    return values[state->median_count / 2U];
}

static int32_t rate_step(uint16_t rate, uint32_t dt_ms) {
    uint32_t step = (uint32_t)rate * dt_ms * 256U / 20U;
    return (int32_t)(step > 0U ? step : 1U);
}

static int32_t clamp_step(int32_t value, int32_t limit) {
    if (value < -limit) return -limit;
    if (value > limit) return limit;
    return value;
}

static uint16_t add_evidence(uint16_t value, uint32_t dt_ms) {
    uint32_t result = (uint32_t)value + dt_ms;
    return result > UINT16_MAX ? UINT16_MAX : (uint16_t)result;
}

static uint16_t motion_magnitude(int16_t x, int16_t y) {
    uint32_t magnitude = (uint32_t)(x < 0 ? -(int32_t)x : x) +
                         (uint32_t)(y < 0 ? -(int32_t)y : y);
    return magnitude > UINT16_MAX ? UINT16_MAX : (uint16_t)magnitude;
}

static fsr_sentinel_output_t successor_step(
    fsr_sentinel_successor_t *state, const fsr_sentinel_v2_params_t *params,
    const fsr_sentinel_v3_params_t *motion_params, uint16_t reading,
    uint32_t t_ms, int16_t motion_x, int16_t motion_y) {
    bool motion_recent = false;
    if (motion_params != NULL) {
        if (motion_magnitude(motion_x, motion_y) >=
            motion_params->motion_threshold) {
            state->motion_until_ms = t_ms + motion_params->motion_hold_ms;
            state->motion_seen = true;
        }
        motion_recent = state->motion_seen &&
                        state->motion_until_ms - t_ms < 0x80000000UL;
    }

    uint16_t filtered = median_step(state, reading);
    fsr_sentinel_reason_t reason = FSR_SENTINEL_REASON_NONE;
    if (!state->initialized) {
        state->initialized = true;
        state->center_q8 = (int32_t)filtered << 8;
        state->peak_q8 = (int32_t)filtered << 8;
        state->prior_t_ms = t_ms;
        state->prior_filtered = filtered;
        return (fsr_sentinel_output_t){false, state->state,
                                       FSR_SENTINEL_REASON_SEED};
    }

    uint32_t dt_ms = t_ms - state->prior_t_ms;
    state->prior_t_ms = t_ms;
    if (dt_ms == 0U) dt_ms = 1U;
    bool gap = dt_ms > params->gap_release_ms;
    uint32_t evidence_ms = dt_ms < 20U ? dt_ms : 20U;
    if (gap) {
        state->touch_evidence_ms = 0;
        state->release_evidence_ms = 0;
        if (state->state == FSR_SENTINEL_TOUCHED) {
            state->state = FSR_SENTINEL_RECOVERY;
            state->recovery_started_ms = t_ms;
            reason = FSR_SENTINEL_REASON_SCAN_GAP;
        }
    }

    if (state->state == FSR_SENTINEL_IDLE) {
        int32_t target = (int32_t)filtered << 8;
        int32_t limit = rate_step(params->center_rate, evidence_ms);
        state->center_q8 += clamp_step(target - state->center_q8, limit);
        int32_t residual = (int32_t)filtered - (state->center_q8 >> 8);
        uint16_t threshold = motion_recent ? motion_params->motion_touch_delta
                                           : params->touch_delta;
        if (residual >= threshold) {
            state->touch_evidence_ms =
                add_evidence(state->touch_evidence_ms, evidence_ms);
        } else {
            state->touch_evidence_ms = 0;
        }
        if (state->touch_evidence_ms >= params->touch_confirm_ms) {
            state->state = FSR_SENTINEL_TOUCHED;
            state->touch_anchor = (int16_t)(state->center_q8 >> 8);
            state->peak_q8 = (int32_t)filtered << 8;
            state->touch_evidence_ms = 0;
            state->release_evidence_ms = 0;
            reason = FSR_SENTINEL_REASON_ENVELOPE_TOUCH;
        }
    } else if (state->state == FSR_SENTINEL_TOUCHED) {
        int32_t target = (int32_t)filtered << 8;
        if (target >= state->peak_q8) {
            state->peak_q8 = target;
        } else {
            int32_t next = state->peak_q8 -
                           rate_step(params->peak_decay, evidence_ms);
            state->peak_q8 = next > target ? next : target;
        }
        int32_t drop = (state->peak_q8 >> 8) - filtered;
        bool near_floor = (int32_t)filtered <=
                          (int32_t)state->touch_anchor +
                              params->release_floor_margin;
        if (drop >= params->release_drop || near_floor) {
            state->release_evidence_ms =
                add_evidence(state->release_evidence_ms, evidence_ms);
        } else {
            state->release_evidence_ms = 0;
        }
        uint32_t required = params->release_confirm_ms;
        if (motion_recent) {
            required += motion_params->motion_release_extra_ms;
        }
        if (state->release_evidence_ms >= required) {
            state->state = FSR_SENTINEL_RECOVERY;
            state->recovery_started_ms = t_ms;
            state->release_evidence_ms = 0;
            reason = FSR_SENTINEL_REASON_ENVELOPE_RELEASE;
        }
    } else if (t_ms - state->recovery_started_ms >=
               params->recovery_blank_ms) {
        state->state = FSR_SENTINEL_IDLE;
        state->center_q8 = (int32_t)filtered << 8;
        state->peak_q8 = (int32_t)filtered << 8;
        state->touch_evidence_ms = 0;
        state->release_evidence_ms = 0;
        reason = FSR_SENTINEL_REASON_RECOVERY_COMPLETE;
    }

    state->prior_filtered = filtered;
    return (fsr_sentinel_output_t){
        state->state == FSR_SENTINEL_TOUCHED, state->state, reason};
}

void fsr_sentinel_runtime_init(fsr_sentinel_runtime_t *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->algorithm = (fsr_sentinel_algorithm_id_t)active_algorithm;
    if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_V1) {
        fsr_sentinel_init(&runtime->detector.v1);
    } else if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        fsr_atlas_phase_reset(&runtime->detector.atlas_phase);
    } else {
        runtime->detector.successor.state = FSR_SENTINEL_IDLE;
    }
}

fsr_sentinel_output_t fsr_sentinel_runtime_step(
    fsr_sentinel_runtime_t *runtime, uint16_t reading, uint32_t t_ms,
    uint32_t scan, int16_t motion_x, int16_t motion_y) {
    if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_V1) {
        return fsr_sentinel_step(&runtime->detector.v1, reading, t_ms, scan);
    }
    if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_V2) {
        return successor_step(&runtime->detector.successor,
                              &g_fsr_sentinel_v2_params, NULL, reading, t_ms,
                              motion_x, motion_y);
    }
    if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        bool initialized = runtime->detector.atlas_phase.initialized;
        fsr_atlas_phase_output_t output = fsr_atlas_phase_step(
            &runtime->detector.atlas_phase, reading, t_ms, motion_x, motion_y);
        fsr_sentinel_reason_t reason = FSR_SENTINEL_REASON_NONE;
        if (!initialized) {
            reason = FSR_SENTINEL_REASON_SEED;
        } else if (output.gap_reset) {
            reason = FSR_SENTINEL_REASON_SCAN_GAP;
        } else if (output.changed) {
            reason = output.touched ? FSR_SENTINEL_REASON_ENVELOPE_TOUCH
                                    : FSR_SENTINEL_REASON_ENVELOPE_RELEASE;
        }
        return (fsr_sentinel_output_t){
            output.touched,
            output.touched ? FSR_SENTINEL_TOUCHED : FSR_SENTINEL_IDLE,
            reason,
        };
    }
    return successor_step(&runtime->detector.successor,
                          &g_fsr_sentinel_v3_params.signal,
                          &g_fsr_sentinel_v3_params, reading, t_ms, motion_x,
                          motion_y);
}

int32_t fsr_sentinel_runtime_center(const fsr_sentinel_runtime_t *runtime) {
    if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        return fsr_atlas_phase_center(&runtime->detector.atlas_phase);
    }
    return runtime->algorithm == FSR_SENTINEL_ALGORITHM_V1
               ? runtime->detector.v1.center_q8 >> 8
               : runtime->detector.successor.center_q8 >> 8;
}

uint16_t fsr_sentinel_runtime_filtered(const fsr_sentinel_runtime_t *runtime) {
    if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        return fsr_atlas_phase_filtered(&runtime->detector.atlas_phase);
    }
    return runtime->algorithm == FSR_SENTINEL_ALGORITHM_V1
               ? runtime->detector.v1.prior_filtered
               : runtime->detector.successor.prior_filtered;
}

uint16_t fsr_sentinel_runtime_touch_evidence(
    const fsr_sentinel_runtime_t *runtime) {
    if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        int32_t evidence =
            fsr_atlas_phase_touch_evidence(&runtime->detector.atlas_phase);
        return evidence <= 0 ? 0U
                             : evidence > UINT16_MAX ? UINT16_MAX
                                                     : (uint16_t)evidence;
    }
    return runtime->algorithm == FSR_SENTINEL_ALGORITHM_V1
               ? runtime->detector.v1.positive_score
               : runtime->detector.successor.touch_evidence_ms;
}

uint16_t fsr_sentinel_runtime_release_evidence(
    const fsr_sentinel_runtime_t *runtime) {
    if (runtime->algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1) {
        int32_t evidence =
            fsr_atlas_phase_release_evidence(&runtime->detector.atlas_phase);
        return evidence <= 0 ? 0U
                             : evidence > UINT16_MAX ? UINT16_MAX
                                                     : (uint16_t)evidence;
    }
    return runtime->algorithm == FSR_SENTINEL_ALGORITHM_V1
               ? runtime->detector.v1.negative_score
               : runtime->detector.successor.release_evidence_ms;
}

uint16_t fsr_sentinel_runtime_interval_ms(
    const fsr_sentinel_runtime_t *runtime, uint16_t legacy_interval_ms) {
    return runtime != NULL &&
                   runtime->algorithm == FSR_SENTINEL_ALGORITHM_ATLAS_PHASE_V1
               ? fsr_atlas_phase_interval_ms()
               : legacy_interval_ms;
}
