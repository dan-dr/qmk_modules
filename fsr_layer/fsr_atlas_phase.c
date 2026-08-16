// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fsr_atlas_phase.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

/* Live-tuned 2026-08-12 EEPROM snapshot (active atlas_phase_v1):
 * motion_window_ms 5, motion_force_floor 10, motion_threshold 5,
 * baseline_tau_ms 1500, onset_rise 45, release_fall 40, onset_residual 70,
 * release_residual 40, release_dwell_ms 5, retouch_rise 40. */
static const fsr_atlas_phase_params_t default_params = {
    FSR_ATLAS_FRONTEND_RAW, 1, 1, 100, 1, 5, 10, 5,
    1500, 8, 45, 40, 70, 40, 0, 5, 40,
};

fsr_atlas_phase_params_t g_fsr_atlas_phase_params = {
    FSR_ATLAS_FRONTEND_RAW, 1, 1, 100, 1, 5, 10, 5,
    1500, 8, 45, 40, 70, 40, 0, 5, 40,
};

typedef struct {
    bool desired;
    bool near_on;
    uint16_t on_dwell_ms;
    uint16_t off_dwell_ms;
} decision_t;

static int32_t clamp_i32(int64_t value) {
    if (value > INT32_MAX) return INT32_MAX;
    if (value < INT32_MIN) return INT32_MIN;
    return (int32_t)value;
}

static int32_t max_i32(int32_t left, int32_t right) {
    return left > right ? left : right;
}

static int32_t min_i32(int32_t left, int32_t right) {
    return left < right ? left : right;
}

static int32_t bounded_i32(int32_t value, int32_t low, int32_t high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static uint16_t bounded_u16(int16_t value, uint16_t low, uint16_t high) {
    if (value < (int16_t)low) return low;
    if (value > (int16_t)high) return high;
    return (uint16_t)value;
}

/* bit0 enter, bit1 hold, bit2 rolling alone (no press / near_on required). */
static uint8_t motion_mode_bits(void) {
    return (uint8_t)g_fsr_atlas_phase_params.motion_mode & 7U;
}

static bool motion_is_recent(const fsr_atlas_phase_state_t *state, uint32_t now) {
    return (uint32_t)(now - state->last_motion_ms) <=
           bounded_u16(g_fsr_atlas_phase_params.motion_window_ms, 1, 500);
}

static bool motion_force_ok(bool near_on, bool credible_force, uint8_t motion_mode) {
    if ((motion_mode & 4U) != 0U) return true;
    return near_on && credible_force;
}

static int32_t q8_from_int(int32_t value) {
    return clamp_i32((int64_t)value * 256);
}

static int32_t q8_to_int(int32_t value) {
    return value / 256;
}

static int32_t ewma_update(int32_t current_q8, int32_t target,
                           uint32_t dt_ms, uint32_t tau_ms) {
    const int64_t target_q8 = (int64_t)target * 256;
    const uint64_t denominator = (uint64_t)tau_ms + dt_ms;
    const int64_t adjustment = (target_q8 - current_q8) * dt_ms /
                               (int64_t)(denominator == 0 ? 1 : denominator);
    return clamp_i32((int64_t)current_q8 + adjustment);
}

static uint16_t cadence_ms(void) {
    return bounded_u16(g_fsr_atlas_phase_params.cadence_ms, 1, 20);
}

static uint16_t gap_ms(void) {
    return bounded_u16(g_fsr_atlas_phase_params.gap_ms, 20, 1000);
}

static uint16_t dwell_ms(int16_t value) {
    return bounded_u16(value, 0, 1000);
}

static uint16_t frontend_width(void) {
    uint16_t width = bounded_u16(g_fsr_atlas_phase_params.frontend_param, 1,
                                 FSR_ATLAS_PHASE_FRONTEND_COUNT);
    width |= 1U;
    return width > FSR_ATLAS_PHASE_FRONTEND_COUNT
               ? FSR_ATLAS_PHASE_FRONTEND_COUNT
               : width;
}

static uint16_t frontend_step(fsr_atlas_phase_state_t *state, uint16_t reading,
                              uint32_t dt_ms) {
    int16_t kind = g_fsr_atlas_phase_params.frontend;
    if (kind < FSR_ATLAS_FRONTEND_RAW ||
        kind > FSR_ATLAS_FRONTEND_EXPONENTIAL) {
        kind = FSR_ATLAS_FRONTEND_RAW;
    }
    if (kind == FSR_ATLAS_FRONTEND_RAW) return reading;
    if (kind == FSR_ATLAS_FRONTEND_MEAN ||
        kind == FSR_ATLAS_FRONTEND_MEDIAN) {
        const uint16_t width = frontend_width();
        if (state->front_count < width) {
            state->front[state->front_count++] = (int16_t)reading;
            state->front_sum += reading;
        } else {
            state->front_sum -= state->front[state->front_cursor];
            state->front[state->front_cursor] = (int16_t)reading;
            state->front_sum += reading;
            state->front_cursor = (uint8_t)((state->front_cursor + 1U) % width);
        }
        if (kind == FSR_ATLAS_FRONTEND_MEAN) {
            return (uint16_t)(state->front_sum / state->front_count);
        }
        int16_t ordered[FSR_ATLAS_PHASE_FRONTEND_COUNT];
        memcpy(ordered, state->front,
               (size_t)state->front_count * sizeof(ordered[0]));
        for (uint8_t i = 1; i < state->front_count; ++i) {
            const int16_t value = ordered[i];
            uint8_t j = i;
            while (j > 0 && ordered[j - 1U] > value) {
                ordered[j] = ordered[j - 1U];
                --j;
            }
            ordered[j] = value;
        }
        return (uint16_t)ordered[state->front_count / 2U];
    }
    if (kind == FSR_ATLAS_FRONTEND_SLEW_LIMITED) {
        const int32_t current = q8_to_int(state->front_value_q8);
        const int32_t rate = bounded_u16(
            g_fsr_atlas_phase_params.frontend_param, 1, 200);
        const int32_t max_step = clamp_i32((int64_t)rate * dt_ms);
        int32_t delta = (int32_t)reading - current;
        if (delta > max_step) delta = max_step;
        if (delta < -max_step) delta = -max_step;
        state->front_value_q8 = clamp_i32(
            (int64_t)state->front_value_q8 + (int64_t)delta * 256);
    } else {
        const uint16_t tau = bounded_u16(
            g_fsr_atlas_phase_params.frontend_param, 2, 1000);
        state->front_value_q8 = ewma_update(state->front_value_q8, reading,
                                            dt_ms, tau);
    }
    return (uint16_t)q8_to_int(state->front_value_q8);
}

static void push_history(fsr_atlas_phase_state_t *state, uint16_t value) {
    ++state->count;
    state->history[state->count % FSR_ATLAS_PHASE_HISTORY_COUNT] =
        (int16_t)value;
}

static int32_t lag_value(const fsr_atlas_phase_state_t *state, uint32_t lag) {
    if (state->count == 0) return state->filtered;
    if (lag >= state->count) lag = state->count - 1U;
    return state->history[(state->count - lag) %
                          FSR_ATLAS_PHASE_HISTORY_COUNT];
}

static uint32_t excursion_width(void) {
    const uint32_t cadence = cadence_ms();
    int32_t physical = g_fsr_atlas_phase_params.p1;
    if (physical < 1) physical = 1;
    if (physical > (int32_t)(64U * cadence)) physical = (int32_t)(64U * cadence);
    return ((uint32_t)physical + cadence - 1U) / cadence;
}

static void extrema_last(const fsr_atlas_phase_state_t *state, uint32_t width,
                         int32_t *low, int32_t *high) {
    uint32_t available = state->count;
    if (available > FSR_ATLAS_PHASE_HISTORY_COUNT - 1U) {
        available = FSR_ATLAS_PHASE_HISTORY_COUNT - 1U;
    }
    if (width > available) width = available;
    *low = lag_value(state, 0);
    *high = *low;
    for (uint32_t lag = 1; lag < width; ++lag) {
        *low = min_i32(*low, lag_value(state, lag));
        *high = max_i32(*high, lag_value(state, lag));
    }
}

static bool dwell(fsr_atlas_phase_state_t *state, bool desired, uint32_t now,
                  uint16_t on_ms, uint16_t off_ms) {
    if (desired == state->touched) {
        state->pending_valid = false;
        return state->touched;
    }
    const uint16_t needed = desired ? on_ms : off_ms;
    if (needed == 0) {
        state->touched = desired;
        state->pending_valid = false;
        return state->touched;
    }
    if (!state->pending_valid || state->pending_value != desired) {
        state->pending_valid = true;
        state->pending_value = desired;
        state->pending_since = now;
        return state->touched;
    }
    if ((uint32_t)(now - state->pending_since) >= needed) {
        state->touched = desired;
        state->pending_valid = false;
    }
    return state->touched;
}

static decision_t phase_decision(fsr_atlas_phase_state_t *state, int32_t filtered,
                                 uint32_t now, uint32_t dt_ms) {
    decision_t decision = {false, false, dwell_ms(g_fsr_atlas_phase_params.p6),
                           dwell_ms(g_fsr_atlas_phase_params.p7)};
    if (state->family_saw_touched && !state->touched) {
        state->quarantine_active = true;
        state->quarantine_since = now;
        state->release_floor = filtered;
    }
    int32_t low = filtered;
    int32_t high = filtered;
    extrema_last(state, excursion_width(), &low, &high);
    const int32_t residual = filtered - q8_to_int(state->base_q8);
    const int32_t rise = filtered - low;
    const int32_t fall = high - filtered;
    const int32_t onset_residual = bounded_i32(
        g_fsr_atlas_phase_params.p4, 1, 1500);
    const int32_t onset_rise = bounded_i32(
        g_fsr_atlas_phase_params.p2, 1, 1000);
    const int32_t release_fall = bounded_i32(
        g_fsr_atlas_phase_params.p3, 1, 1000);
    int32_t release_residual = bounded_i32(
        g_fsr_atlas_phase_params.p5, -500, onset_residual - 1);
    if (release_residual >= onset_residual) release_residual = onset_residual - 1;
    int32_t retouch_rise = max_i32(1, g_fsr_atlas_phase_params.p8);
    if (retouch_rise > onset_rise) retouch_rise = onset_rise;
    state->on_evidence = max_i32(residual - onset_residual,
                                 rise - onset_rise);
    state->release_evidence = max_i32(release_residual - residual,
                                      fall - release_fall);
    if (state->touched) {
        decision.desired = residual > release_residual && fall < release_fall;
    } else if (state->quarantine_active) {
        state->release_floor = min_i32(state->release_floor, filtered);
        const int32_t rise_from_release = filtered - state->release_floor;
        const bool rearmed = (uint32_t)(now - state->quarantine_since) >= 60U;
        const uint8_t motion_mode = motion_mode_bits();
        const bool motion_recent = (motion_mode & 1U) != 0U &&
            motion_is_recent(state, now);
        const int32_t force_floor = bounded_u16(
            g_fsr_atlas_phase_params.motion_force_floor, 1, 500);
        decision.desired = motion_recent &&
            ((motion_mode & 4U) != 0U ||
             (residual >= force_floor && rise_from_release >= retouch_rise));
        if (rearmed) state->quarantine_active = false;
    } else {
        decision.desired = residual >= onset_residual || rise >= onset_rise;
        decision.near_on = residual > 0 && rise >= retouch_rise;
        if (!decision.desired) {
            const uint32_t tau = (uint32_t)bounded_i32(
                g_fsr_atlas_phase_params.p0, 1, 10000);
            state->base_q8 = ewma_update(state->base_q8, filtered, dt_ms, tau);
        }
    }
    if (!state->family_saw_touched && state->touched) {
        state->quarantine_active = false;
        state->release_floor = INT32_MAX;
    }
    state->family_saw_touched = state->touched;
    return decision;
}

void fsr_atlas_phase_params_reset(void) {
    g_fsr_atlas_phase_params = default_params;
}

bool fsr_atlas_phase_params_get(uint8_t index, int16_t *value) {
    if (index >= FSR_ATLAS_PHASE_PARAM_COUNT || value == NULL) return false;
    int16_t values[FSR_ATLAS_PHASE_PARAM_COUNT];
    _Static_assert(sizeof(values) == sizeof(g_fsr_atlas_phase_params),
                   "FSR Atlas phase params must be 17 packed int16 values");
    memcpy(values, &g_fsr_atlas_phase_params, sizeof(values));
    *value = values[index];
    return true;
}

bool fsr_atlas_phase_params_set(uint8_t index, int16_t value) {
    if (index >= FSR_ATLAS_PHASE_PARAM_COUNT) return false;
    int16_t values[FSR_ATLAS_PHASE_PARAM_COUNT];
    memcpy(values, &g_fsr_atlas_phase_params, sizeof(values));
    values[index] = value;
    memcpy(&g_fsr_atlas_phase_params, values, sizeof(values));
    return true;
}

void fsr_atlas_phase_reset(fsr_atlas_phase_state_t *state) {
    if (state != NULL) memset(state, 0, sizeof(*state));
}

void fsr_atlas_phase_init(fsr_atlas_phase_state_t *state, uint16_t reading,
                          uint32_t t_ms) {
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->initialized = true;
    state->last_sample_ms = t_ms;
    state->last_eval_ms = t_ms;
    state->last_motion_ms = t_ms - 60000U;
    state->front_value_q8 = q8_from_int(reading);
    state->gate_base_q8 = q8_from_int(reading);
    state->base_q8 = q8_from_int(reading);
    state->release_floor = INT32_MAX;
    state->filtered = reading;
}

fsr_atlas_phase_output_t fsr_atlas_phase_step(
    fsr_atlas_phase_state_t *state, uint16_t reading, uint32_t t_ms,
    int16_t motion_x, int16_t motion_y) {
    fsr_atlas_phase_output_t output = {false, false, false, false, reading};
    if (state == NULL) return output;
    if (!state->initialized) fsr_atlas_phase_init(state, reading, t_ms);
    const bool touched_before = state->touched;
    const uint32_t sample_dt = t_ms - state->last_sample_ms;
    if (sample_dt > gap_ms()) {
        fsr_atlas_phase_init(state, reading, t_ms);
        output.gap_reset = true;
    }
    state->last_sample_ms = t_ms;
    const int32_t motion = (motion_x < 0 ? -(int32_t)motion_x : motion_x) +
                           (motion_y < 0 ? -(int32_t)motion_y : motion_y);
    const uint16_t motion_threshold = bounded_u16(
        g_fsr_atlas_phase_params.motion_threshold, 1, 1000);
    if (motion >= motion_threshold) state->last_motion_ms = t_ms;
    const uint32_t elapsed = t_ms - state->last_eval_ms;
    if (elapsed >= cadence_ms()) {
        state->last_eval_ms = t_ms;
        state->last_cadence_ms = elapsed;
        output.evaluated = true;
        state->filtered = frontend_step(state, reading, elapsed);
        push_history(state, state->filtered);
        decision_t decision = phase_decision(state, state->filtered, t_ms,
                                             elapsed);
        const bool motion_recent = motion_is_recent(state, t_ms);
        const bool credible_force =
            (int32_t)state->filtered - q8_to_int(state->gate_base_q8) >=
            bounded_u16(g_fsr_atlas_phase_params.motion_force_floor, 1, 500);
        const uint8_t motion_mode = motion_mode_bits();
        const bool motion_ok = motion_recent &&
            motion_force_ok(decision.near_on, credible_force, motion_mode);
        if (!state->touched && !decision.desired &&
            (motion_mode & 1U) != 0U && motion_ok) {
            decision.desired = true;
            decision.on_dwell_ms /= 2U;
        }
        if (state->touched && !decision.desired &&
            (motion_mode & 2U) != 0U && motion_ok) {
            decision.desired = true;
        }
        dwell(state, decision.desired, t_ms, decision.on_dwell_ms,
              decision.off_dwell_ms);
        if (!state->touched) {
            state->gate_base_q8 = ewma_update(state->gate_base_q8,
                                              state->filtered, elapsed, 800);
        }
    }
    output.touched = state->touched;
    output.changed = touched_before != state->touched;
    output.filtered = state->filtered;
    return output;
}

int32_t fsr_atlas_phase_center(const fsr_atlas_phase_state_t *state) {
    return state == NULL ? 0 : q8_to_int(state->base_q8);
}

uint16_t fsr_atlas_phase_filtered(const fsr_atlas_phase_state_t *state) {
    return state == NULL ? 0 : state->filtered;
}

int32_t fsr_atlas_phase_touch_evidence(const fsr_atlas_phase_state_t *state) {
    return state == NULL ? 0 : state->on_evidence;
}

int32_t fsr_atlas_phase_release_evidence(const fsr_atlas_phase_state_t *state) {
    return state == NULL ? 0 : state->release_evidence;
}

uint32_t fsr_atlas_phase_cadence_ms(const fsr_atlas_phase_state_t *state) {
    return state == NULL ? 0 : state->last_cadence_ms;
}

uint16_t fsr_atlas_phase_interval_ms(void) {
    return cadence_ms();
}
