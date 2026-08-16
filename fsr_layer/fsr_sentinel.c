// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fsr_sentinel.h"

#include <string.h>

enum {
    FSR_SENTINEL_SCORE_MAX = 65535,
};

_Static_assert(
    sizeof(fsr_sentinel_t) <= FSR_SENTINEL_STATE_BUDGET_BYTES,
    "FSR Sentinel state exceeds the run7-provisional-v1 firmware budget");
_Static_assert(FSR_SENTINEL_CENTER_RATE_PER_20_MS > 0U &&
                   FSR_SENTINEL_CENTER_RATE_PER_20_MS <= 4095U,
               "FSR Sentinel center rate must fit the ADC range");
_Static_assert(FSR_SENTINEL_RECOVERY_BLANK_MS > 0U,
               "FSR Sentinel recovery blank must be positive");
_Static_assert(FSR_SENTINEL_RELEASE_DRIFT >= 0 &&
                   FSR_SENTINEL_RELEASE_DRIFT <= 4095,
               "FSR Sentinel release drift must fit the ADC range");
_Static_assert(FSR_SENTINEL_RELEASE_SCORE > 0U &&
                   FSR_SENTINEL_RELEASE_SCORE <= UINT16_MAX,
               "FSR Sentinel release score must fit uint16_t");
_Static_assert(FSR_SENTINEL_SCORE_LEAK_PER_20_MS >= 0 &&
                   FSR_SENTINEL_SCORE_LEAK_PER_20_MS <= UINT16_MAX,
               "FSR Sentinel score leak must fit uint16_t");
_Static_assert(FSR_SENTINEL_TOUCH_DRIFT >= 0 &&
                   FSR_SENTINEL_TOUCH_DRIFT <= 4095,
               "FSR Sentinel touch drift must fit the ADC range");
_Static_assert(FSR_SENTINEL_TOUCH_SCORE > 0U &&
                   FSR_SENTINEL_TOUCH_SCORE <= UINT16_MAX,
               "FSR Sentinel touch score must fit uint16_t");
_Static_assert(FSR_SENTINEL_PARAM_COUNT <= 16,
               "FSR Sentinel HID GET_ALL packs at most 16 int16 values");

fsr_sentinel_params_t g_fsr_sentinel_params = {
    .center_rate_per_20_ms = FSR_SENTINEL_CENTER_RATE_PER_20_MS,
    .recovery_blank_ms     = FSR_SENTINEL_RECOVERY_BLANK_MS,
    .release_drift         = FSR_SENTINEL_RELEASE_DRIFT,
    .release_score         = FSR_SENTINEL_RELEASE_SCORE,
    .score_leak_per_20_ms  = FSR_SENTINEL_SCORE_LEAK_PER_20_MS,
    .touch_drift           = FSR_SENTINEL_TOUCH_DRIFT,
    .touch_score           = FSR_SENTINEL_TOUCH_SCORE,
};

/* Registry is the schema source for WebHID. Add new parameters here. */
static const fsr_sentinel_param_meta_t fsr_sentinel_param_table
    [FSR_SENTINEL_PARAM_COUNT] = {
        {FSR_SENTINEL_PARAM_CENTER_RATE, "center_rate",
         "Center rate (ADC / 20 ms)", 1, 4095,
         (int16_t)FSR_SENTINEL_CENTER_RATE_PER_20_MS},
        {FSR_SENTINEL_PARAM_RECOVERY_BLANK_MS, "recovery_blank_ms",
         "Recovery blank (ms)", 1, 5000,
         (int16_t)FSR_SENTINEL_RECOVERY_BLANK_MS},
        {FSR_SENTINEL_PARAM_RELEASE_DRIFT, "release_drift",
         "Release drift (ADC)", 0, 4095, (int16_t)FSR_SENTINEL_RELEASE_DRIFT},
        {FSR_SENTINEL_PARAM_RELEASE_SCORE, "release_score", "Release score", 1,
         32767, (int16_t)FSR_SENTINEL_RELEASE_SCORE},
        {FSR_SENTINEL_PARAM_SCORE_LEAK, "score_leak", "Score leak / 20 ms", 0,
         32767, (int16_t)FSR_SENTINEL_SCORE_LEAK_PER_20_MS},
        {FSR_SENTINEL_PARAM_TOUCH_DRIFT, "touch_drift", "Touch drift (ADC)", 0,
         4095, (int16_t)FSR_SENTINEL_TOUCH_DRIFT},
        {FSR_SENTINEL_PARAM_TOUCH_SCORE, "touch_score", "Touch score", 1, 32767,
         (int16_t)FSR_SENTINEL_TOUCH_SCORE},
};

void fsr_sentinel_params_reset(void) {
    g_fsr_sentinel_params.center_rate_per_20_ms =
        FSR_SENTINEL_CENTER_RATE_PER_20_MS;
    g_fsr_sentinel_params.recovery_blank_ms = FSR_SENTINEL_RECOVERY_BLANK_MS;
    g_fsr_sentinel_params.release_drift     = FSR_SENTINEL_RELEASE_DRIFT;
    g_fsr_sentinel_params.release_score     = FSR_SENTINEL_RELEASE_SCORE;
    g_fsr_sentinel_params.score_leak_per_20_ms =
        FSR_SENTINEL_SCORE_LEAK_PER_20_MS;
    g_fsr_sentinel_params.touch_drift = FSR_SENTINEL_TOUCH_DRIFT;
    g_fsr_sentinel_params.touch_score = FSR_SENTINEL_TOUCH_SCORE;
}

const fsr_sentinel_param_meta_t *fsr_sentinel_param_meta(uint8_t id) {
    if (id >= FSR_SENTINEL_PARAM_COUNT) {
        return NULL;
    }
    return &fsr_sentinel_param_table[id];
}

bool fsr_sentinel_param_get(uint8_t id, int16_t *value) {
    if (value == NULL || id >= FSR_SENTINEL_PARAM_COUNT) {
        return false;
    }
    switch ((fsr_sentinel_param_id_t)id) {
        case FSR_SENTINEL_PARAM_CENTER_RATE:
            *value = (int16_t)g_fsr_sentinel_params.center_rate_per_20_ms;
            return true;
        case FSR_SENTINEL_PARAM_RECOVERY_BLANK_MS:
            *value = (int16_t)g_fsr_sentinel_params.recovery_blank_ms;
            return true;
        case FSR_SENTINEL_PARAM_RELEASE_DRIFT:
            *value = g_fsr_sentinel_params.release_drift;
            return true;
        case FSR_SENTINEL_PARAM_RELEASE_SCORE:
            *value = (int16_t)g_fsr_sentinel_params.release_score;
            return true;
        case FSR_SENTINEL_PARAM_SCORE_LEAK:
            *value = g_fsr_sentinel_params.score_leak_per_20_ms;
            return true;
        case FSR_SENTINEL_PARAM_TOUCH_DRIFT:
            *value = g_fsr_sentinel_params.touch_drift;
            return true;
        case FSR_SENTINEL_PARAM_TOUCH_SCORE:
            *value = (int16_t)g_fsr_sentinel_params.touch_score;
            return true;
        case FSR_SENTINEL_PARAM_COUNT:
            break;
    }
    return false;
}

bool fsr_sentinel_param_set(uint8_t id, int16_t value) {
    const fsr_sentinel_param_meta_t *meta = fsr_sentinel_param_meta(id);
    if (meta == NULL || value < meta->min || value > meta->max) {
        return false;
    }
    switch ((fsr_sentinel_param_id_t)id) {
        case FSR_SENTINEL_PARAM_CENTER_RATE:
            g_fsr_sentinel_params.center_rate_per_20_ms = (uint16_t)value;
            return true;
        case FSR_SENTINEL_PARAM_RECOVERY_BLANK_MS:
            g_fsr_sentinel_params.recovery_blank_ms = (uint16_t)value;
            return true;
        case FSR_SENTINEL_PARAM_RELEASE_DRIFT:
            g_fsr_sentinel_params.release_drift = value;
            return true;
        case FSR_SENTINEL_PARAM_RELEASE_SCORE:
            g_fsr_sentinel_params.release_score = (uint16_t)value;
            return true;
        case FSR_SENTINEL_PARAM_SCORE_LEAK:
            g_fsr_sentinel_params.score_leak_per_20_ms = value;
            return true;
        case FSR_SENTINEL_PARAM_TOUCH_DRIFT:
            g_fsr_sentinel_params.touch_drift = value;
            return true;
        case FSR_SENTINEL_PARAM_TOUCH_SCORE:
            g_fsr_sentinel_params.touch_score = (uint16_t)value;
            return true;
        case FSR_SENTINEL_PARAM_COUNT:
            break;
    }
    return false;
}

static uint16_t fsr_sentinel_median_step(fsr_sentinel_t *sentinel,
                                         uint16_t reading) {
    sentinel->median_values[sentinel->median_next] = reading;
    sentinel->median_next = (uint8_t)((sentinel->median_next + 1U) % 3U);
    if (sentinel->median_count < 3U) {
        sentinel->median_count++;
    }

    uint16_t values[3];
    for (uint8_t index = 0; index < sentinel->median_count; index++) {
        values[index] = sentinel->median_values[index];
    }
    for (uint8_t left = 1; left < sentinel->median_count; left++) {
        uint16_t value = values[left];
        uint8_t right  = left;
        while (right > 0U && values[right - 1U] > value) {
            values[right] = values[right - 1U];
            right--;
        }
        values[right] = value;
    }
    return values[sentinel->median_count / 2U];
}

static int32_t fsr_sentinel_q8_step(uint32_t rate_per_20_ms,
                                    uint32_t dt_ms) {
    if (dt_ms == 0U) {
        dt_ms = 1U;
    }
    int32_t step = (int32_t)(rate_per_20_ms * dt_ms * 256U / 20U);
    return step > 0 ? step : 1;
}

static uint16_t fsr_sentinel_saturating_score(int32_t value) {
    if (value <= 0) {
        return 0;
    }
    if (value >= FSR_SENTINEL_SCORE_MAX) {
        return FSR_SENTINEL_SCORE_MAX;
    }
    return (uint16_t)value;
}

static int32_t fsr_sentinel_clamp_step(int32_t value, int32_t limit) {
    if (value < -limit) {
        return -limit;
    }
    if (value > limit) {
        return limit;
    }
    return value;
}

void fsr_sentinel_init(fsr_sentinel_t *sentinel) {
    memset(sentinel, 0, sizeof(*sentinel));
    sentinel->state = FSR_SENTINEL_IDLE;
}

fsr_sentinel_output_t fsr_sentinel_step(fsr_sentinel_t *sentinel,
                                        uint16_t reading,
                                        uint32_t t_ms,
                                        uint32_t scan) {
    const fsr_sentinel_params_t *params = &g_fsr_sentinel_params;
    uint16_t filtered = fsr_sentinel_median_step(sentinel, reading);
    fsr_sentinel_reason_t reason = FSR_SENTINEL_REASON_NONE;
    if (!sentinel->initialized) {
        sentinel->center_q8      = (int32_t)filtered << 8;
        sentinel->prior_filtered = filtered;
        sentinel->prior_t_ms     = t_ms;
        sentinel->prior_scan     = scan;
        sentinel->initialized    = true;
        reason                   = FSR_SENTINEL_REASON_SEED;
        return (fsr_sentinel_output_t){false, sentinel->state, reason};
    }

    uint32_t dt_ms = t_ms - sentinel->prior_t_ms;
    if (dt_ms == 0U) {
        dt_ms = 1U;
    }
    uint32_t evidence_dt_ms = dt_ms < 20U ? dt_ms : 20U;
    bool gap = scan != sentinel->prior_scan + 1U;
    sentinel->prior_scan = scan;
    sentinel->prior_t_ms = t_ms;
    if (gap) {
        sentinel->positive_score = 0;
        sentinel->negative_score = 0;
    }

    int32_t center = sentinel->center_q8 >> 8;
    if (sentinel->state == FSR_SENTINEL_IDLE) {
        int32_t residual = (int32_t)filtered - center;
        int32_t evidence = residual - params->touch_drift;
        if (evidence < 0) {
            evidence = 0;
        }
        int32_t accumulated =
            (int32_t)sentinel->positive_score +
            evidence * (int32_t)evidence_dt_ms / 20 -
            params->score_leak_per_20_ms * (int32_t)evidence_dt_ms / 20;
        sentinel->positive_score =
            fsr_sentinel_saturating_score(accumulated);
        if (sentinel->positive_score >= params->touch_score) {
            sentinel->state          = FSR_SENTINEL_TOUCHED;
            sentinel->positive_score = 0;
            sentinel->negative_score = 0;
            reason = FSR_SENTINEL_REASON_POSITIVE_CUSUM;
        } else {
            int32_t limit = fsr_sentinel_q8_step(
                params->center_rate_per_20_ms, evidence_dt_ms);
            int32_t target = (int32_t)filtered << 8;
            sentinel->center_q8 += fsr_sentinel_clamp_step(
                target - sentinel->center_q8, limit);
        }
    } else if (sentinel->state == FSR_SENTINEL_TOUCHED) {
        int32_t negative_step = (int32_t)sentinel->prior_filtered -
                                (int32_t)filtered - params->release_drift;
        if (negative_step < 0) {
            negative_step = 0;
        }
        int32_t accumulated =
            (int32_t)sentinel->negative_score +
            negative_step * (int32_t)evidence_dt_ms / 20 -
            params->score_leak_per_20_ms * (int32_t)evidence_dt_ms / 20;
        sentinel->negative_score =
            fsr_sentinel_saturating_score(accumulated);
        if (sentinel->negative_score >= params->release_score) {
            sentinel->state = FSR_SENTINEL_RECOVERY;
            sentinel->recovery_started_ms = t_ms;
            sentinel->positive_score      = 0;
            sentinel->negative_score      = 0;
            reason = FSR_SENTINEL_REASON_NEGATIVE_CUSUM;
        }
    } else if (t_ms - sentinel->recovery_started_ms >=
               params->recovery_blank_ms) {
        sentinel->state          = FSR_SENTINEL_IDLE;
        sentinel->center_q8      = (int32_t)filtered << 8;
        sentinel->positive_score = 0;
        reason = FSR_SENTINEL_REASON_RECOVERY_COMPLETE;
    }

    sentinel->prior_filtered = filtered;
    return (fsr_sentinel_output_t){
        sentinel->state == FSR_SENTINEL_TOUCHED,
        sentinel->state,
        reason,
    };
}

const char *fsr_sentinel_state_name(fsr_sentinel_state_t state) {
    switch (state) {
        case FSR_SENTINEL_IDLE:
            return "idle";
        case FSR_SENTINEL_TOUCHED:
            return "touched";
        case FSR_SENTINEL_RECOVERY:
            return "recovery";
    }
    return "unknown";
}

const char *fsr_sentinel_reason_name(fsr_sentinel_reason_t reason) {
    switch (reason) {
        case FSR_SENTINEL_REASON_NONE:
            return "none";
        case FSR_SENTINEL_REASON_SEED:
            return "seed";
        case FSR_SENTINEL_REASON_POSITIVE_CUSUM:
            return "positive_cusum";
        case FSR_SENTINEL_REASON_NEGATIVE_CUSUM:
            return "negative_cusum";
        case FSR_SENTINEL_REASON_RECOVERY_COMPLETE:
            return "recovery_complete";
        case FSR_SENTINEL_REASON_ENVELOPE_TOUCH:
            return "envelope_touch";
        case FSR_SENTINEL_REASON_ENVELOPE_RELEASE:
            return "envelope_release";
        case FSR_SENTINEL_REASON_SCAN_GAP:
            return "scan_gap";
    }
    return "unknown";
}
