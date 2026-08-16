// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fsr_layer.h"

#include "fsr_sentinel_hid.h"
#include "fsr_sentinel_nvm.h"
#include "fsr_sentinel_runtime.h"
#include "quantum.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 1, 0);

#ifdef FSR_ENABLE
#    include "analog.h"
#    include "debug.h"
#    if defined(FSR_MOUSE_LAYER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
#        include "pointing_device_auto_mouse.h"
#    endif

static bool fsr_touched                      = false;
static bool fsr_active                       = true;
static bool fsr_manual_reset_pending         = false;
static bool fsr_initialized                  = false;
static uint16_t fsr_scan_timer               = 0;
static uint16_t fsr_hid_stream_timer         = 0;
static bool     fsr_hid_last_touch           = false;
#    ifndef FSR_DEBUG_COMPACT
static uint16_t fsr_debug_timer              = 0;
#    endif
static bool     fsr_last_logged_touched      = false;
static uint32_t fsr_scan_count               = 0;
static uint32_t fsr_scan_time_ms             = 0;
static uint32_t fsr_touch_timer              = 0;
static int16_t fsr_last_reading              = 0;
static int16_t fsr_filtered                  = 0;
static int32_t fsr_debug_motion_x            = 0;
static int32_t fsr_debug_motion_y            = 0;
static int32_t fsr_hid_motion_x              = 0;
static int32_t fsr_hid_motion_y              = 0;
static int32_t fsr_sentinel_motion_x         = 0;
static int32_t fsr_sentinel_motion_y         = 0;
#    ifdef FSR_WITNESS_PIN
static int16_t fsr_witness_reading             = 0;
static bool fsr_witness_touched                = false;
static int16_t fsr_adc_probe_26              = 0;
static int16_t fsr_adc_probe_28              = 0;
static int16_t fsr_adc_probe_29              = 0;
#    endif
static fsr_sentinel_runtime_t fsr_sentinel;
static fsr_sentinel_output_t fsr_sentinel_output;
static uint16_t fsr_sentinel_generation      = 0;
static uint32_t fsr_sentinel_last_step_ms    = 0;
static uint32_t fsr_sentinel_scan            = 0;
static uint16_t fsr_sentinel_input           = 0;
static bool fsr_sentinel_has_stepped         = false;
static bool fsr_sentinel_last_gap            = false;

#    ifndef FSR_DEBUG_COMPACT
typedef struct {
    uint32_t scan;
    uint32_t t_ms;
    int16_t  reading;
    int16_t  filtered;
    int16_t  motion_x;
    int16_t  motion_y;
#        ifdef FSR_WITNESS_PIN
    int16_t thumb;
    int16_t p26;
    int16_t p28;
    int16_t p29;
    uint8_t thumb_touched;
#        endif
    uint8_t touched;
    uint8_t state;
} fsr_debug_sample_t;

static fsr_debug_sample_t fsr_debug_buf[FSR_DEBUG_BUFFER_SAMPLES];
static uint8_t            fsr_debug_buf_count  = 0;
static uint16_t           fsr_debug_drop_count = 0;
#    endif

static bool fsr_is_sensor_side(void) {
#    ifdef SPLIT_KEYBOARD
    return is_keyboard_left() == (FSR_ON_LEFT_SIDE != 0);
#    else
    return true;
#    endif
}

static bool fsr_mouse_layer_on(void) {
#    ifdef FSR_MOUSE_LAYER
    return layer_state_is(FSR_MOUSE_LAYER);
#    else
    return false;
#    endif
}

static int16_t fsr_mouse_layer(void) {
#    ifdef FSR_MOUSE_LAYER
    return FSR_MOUSE_LAYER;
#    else
    return -1;
#    endif
}

static bool fsr_auto_mouse_bridge_enabled(void) {
#    if defined(FSR_MOUSE_LAYER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
    return true;
#    else
    return false;
#    endif
}

static void fsr_reset_sentinel(const char *reason) {
    fsr_sentinel_runtime_init(&fsr_sentinel);
    fsr_sentinel_output       = (fsr_sentinel_output_t){0};
    fsr_sentinel_generation   = fsr_sentinel_runtime_generation();
    fsr_sentinel_last_step_ms = 0;
    fsr_sentinel_scan         = 0;
    fsr_sentinel_input        = 0;
    fsr_sentinel_has_stepped  = false;
    fsr_sentinel_last_gap     = false;
    fsr_sentinel_motion_x     = 0;
    fsr_sentinel_motion_y     = 0;
    fsr_hid_motion_x          = 0;
    fsr_hid_motion_y          = 0;
    fsr_hid_stream_timer      = 0;
    fsr_filtered              = 0;
    fsr_touched               = false;
    fsr_touch_timer           = 0;
    fsr_initialized           = true;
    dprintf("FSR SENTINEL RESET scan:%lu t_ms:%lu mode:%s reading:%d\n",
            (unsigned long)fsr_scan_count, (unsigned long)fsr_scan_time_ms,
            reason, fsr_last_reading);
}

static void fsr_log_status(const char *reason) {
    dprintf("FSR status scan:%lu t_ms:%lu [%s]\n",
            (unsigned long)fsr_scan_count, (unsigned long)timer_read32(),
            reason);
    dprintf("  reading:%d filtered:%d touched:%d layer_on:%d active:%d "
            "state:%s motion_x:%ld motion_y:%ld\n",
            fsr_last_reading, fsr_filtered, fsr_touched, fsr_mouse_layer_on(),
            fsr_active, fsr_sentinel_state_name(fsr_sentinel_output.state),
            (long)fsr_debug_motion_x, (long)fsr_debug_motion_y);
    dprintf("  config scan_interval_ms:%d debug_interval_ms:%d "
            "touch_timeout_seconds:%d mouse_layer:%d auto_mouse_bridge:%d\n",
            FSR_SCAN_INTERVAL_MS, FSR_DEBUG_INTERVAL_MS,
            FSR_TOUCH_TIMEOUT_SECONDS, fsr_mouse_layer(),
            fsr_auto_mouse_bridge_enabled());
    dprintf("  sentinel algorithm:%u interval_ms:%d gap_ms:%d scan:%lu "
            "input:%u filtered:%u center:%ld positive_score:%u "
            "negative_score:%u touched:%d state:%s\n",
            fsr_sentinel_runtime_active_algorithm(),
            fsr_sentinel_runtime_interval_ms(&fsr_sentinel,
                                             FSR_SENTINEL_SHADOW_INTERVAL_MS),
            FSR_SENTINEL_SHADOW_GAP_MS, (unsigned long)fsr_sentinel_scan,
            (unsigned int)fsr_sentinel_input,
            (unsigned int)fsr_sentinel_runtime_filtered(&fsr_sentinel),
            (long)fsr_sentinel_runtime_center(&fsr_sentinel),
            (unsigned int)fsr_sentinel_runtime_touch_evidence(&fsr_sentinel),
            (unsigned int)fsr_sentinel_runtime_release_evidence(&fsr_sentinel),
            fsr_sentinel_output.touched,
            fsr_sentinel_state_name(fsr_sentinel_output.state));
#    ifdef FSR_WITNESS_PIN
    dprintf("  thumb pin:%d reading:%d state:%s touch_threshold:%d "
            "mode:witness-only\n",
            FSR_WITNESS_PIN, fsr_witness_reading,
            fsr_witness_touched ? "TOUCHED" : "IDLE",
            FSR_WITNESS_TOUCH_THRESHOLD);
#    endif
}

static void fsr_request_manual_reset(void) {
    if (fsr_touched || fsr_mouse_layer_on()) {
        dprintf("FSR RESET scan:%lu t_ms:%lu manual override releasing "
                "active touch\n",
                (unsigned long)fsr_scan_count,
                (unsigned long)timer_read32());
    }

    if (fsr_initialized) {
        fsr_scan_time_ms = timer_read32();
        fsr_reset_sentinel("manual");
        return;
    }

    fsr_touched              = false;
    fsr_touch_timer          = 0;
    fsr_manual_reset_pending = true;
    dprintf("FSR RESET scan:%lu t_ms:%lu requested; waiting for next %dms "
            "sensor scan\n",
            (unsigned long)fsr_scan_count, (unsigned long)timer_read32(),
            FSR_SCAN_INTERVAL_MS);
}

#    ifdef FSR_DEBUG_COMPACT
static void fsr_debug_log_compact(int16_t reading) {
    if (!debug_enable) {
        fsr_debug_motion_x = 0;
        fsr_debug_motion_y = 0;
        return;
    }
    fsr_last_logged_touched = fsr_touched;
    int16_t mx =
        fsr_debug_motion_x < INT16_MIN
            ? INT16_MIN
            : fsr_debug_motion_x > INT16_MAX ? INT16_MAX
                                             : (int16_t)fsr_debug_motion_x;
    int16_t my =
        fsr_debug_motion_y < INT16_MIN
            ? INT16_MIN
            : fsr_debug_motion_y > INT16_MAX ? INT16_MAX
                                             : (int16_t)fsr_debug_motion_y;
#        ifdef FSR_WITNESS_PIN
    dprintf("%lu,%lu,%d,%u,%d,%d,%d\n", (unsigned long)fsr_scan_count,
            (unsigned long)fsr_scan_time_ms, (int)reading,
            fsr_touched ? 1u : 0u, (int)mx, (int)my, (int)fsr_adc_probe_28);
#        else
    dprintf("%lu,%lu,%d,%u,%d,%d\n", (unsigned long)fsr_scan_count,
            (unsigned long)fsr_scan_time_ms, (int)reading,
            fsr_touched ? 1u : 0u, (int)mx, (int)my);
#        endif
    fsr_debug_motion_x = 0;
    fsr_debug_motion_y = 0;
}
#    else
static const char *fsr_debug_state_name(uint8_t state) {
    switch ((fsr_sentinel_state_t)state) {
        case FSR_SENTINEL_TOUCHED:
            return "touch";
        case FSR_SENTINEL_RECOVERY:
            return "recovery";
        case FSR_SENTINEL_IDLE:
        default:
            return "idle";
    }
}

static void fsr_debug_buffer_sample(int16_t reading) {
    if (!debug_enable) {
        fsr_debug_motion_x = 0;
        fsr_debug_motion_y = 0;
        return;
    }

    fsr_last_logged_touched = fsr_touched;

    if (fsr_debug_buf_count >= FSR_DEBUG_BUFFER_SAMPLES) {
        if (fsr_debug_drop_count < UINT16_MAX) {
            fsr_debug_drop_count++;
        }
        fsr_debug_motion_x = 0;
        fsr_debug_motion_y = 0;
        return;
    }

    fsr_debug_sample_t *sample = &fsr_debug_buf[fsr_debug_buf_count++];
    sample->scan     = fsr_scan_count;
    sample->t_ms     = fsr_scan_time_ms;
    sample->reading  = reading;
    sample->filtered = fsr_filtered;
    sample->motion_x =
        fsr_debug_motion_x < INT16_MIN
            ? INT16_MIN
            : fsr_debug_motion_x > INT16_MAX ? INT16_MAX
                                             : (int16_t)fsr_debug_motion_x;
    sample->motion_y =
        fsr_debug_motion_y < INT16_MIN
            ? INT16_MIN
            : fsr_debug_motion_y > INT16_MAX ? INT16_MAX
                                             : (int16_t)fsr_debug_motion_y;
#        ifdef FSR_WITNESS_PIN
    sample->thumb         = fsr_witness_reading;
    sample->p26           = fsr_adc_probe_26;
    sample->p28           = fsr_adc_probe_28;
    sample->p29           = fsr_adc_probe_29;
    sample->thumb_touched = fsr_witness_touched ? 1 : 0;
#        endif
    sample->touched = fsr_touched ? 1 : 0;
    sample->state   = (uint8_t)fsr_sentinel_output.state;

    fsr_debug_motion_x = 0;
    fsr_debug_motion_y = 0;
}

static void fsr_debug_flush_batch(void) {
    if (!debug_enable || fsr_debug_buf_count == 0) {
        fsr_debug_buf_count  = 0;
        fsr_debug_drop_count = 0;
        return;
    }

    if (fsr_debug_drop_count > 0) {
        dprintf("FSR DEBUG drop:%u (buffer full before flush)\n",
                (unsigned)fsr_debug_drop_count);
    }

    const uint8_t alg = fsr_sentinel_runtime_active_algorithm();
    const uint16_t gen = fsr_sentinel_generation;
    for (uint8_t i = 0; i < fsr_debug_buf_count; i++) {
        const fsr_debug_sample_t *s = &fsr_debug_buf[i];
        dprintf("FSR SCAN scan:%8lu t_ms:%8lu r:%4d f:%4d "
                "touch:%u state:%-8s m:%4d,%4d"
#        ifdef FSR_WITNESS_PIN
                " thumb:%4d/%u@%u p26:%4d p28:%4d p29:%4d"
#        endif
                " alg:%u gen:%u"
                "\n",
                (unsigned long)s->scan, (unsigned long)s->t_ms, s->reading,
                s->filtered, (unsigned)s->touched,
                fsr_debug_state_name(s->state), (int)s->motion_x,
                (int)s->motion_y
#        ifdef FSR_WITNESS_PIN
                ,
                (int)s->thumb, (unsigned)s->thumb_touched,
                (unsigned)FSR_WITNESS_PIN, (int)s->p26, (int)s->p28,
                (int)s->p29
#        endif
                ,
                (unsigned)alg, (unsigned)gen);
    }

    fsr_debug_buf_count  = 0;
    fsr_debug_drop_count = 0;
}
#    endif

static int16_t fsr_saturate_i16(int32_t value) {
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t)value;
}

static void fsr_hid_maybe_send(int16_t reading) {
    bool edge = fsr_touched != fsr_hid_last_touch;
    fsr_hid_last_touch = fsr_touched;
    if (!debug_enable) {
        fsr_hid_stream_timer = 0;
        return;
    }
    if (fsr_hid_stream_timer == 0) {
        fsr_hid_stream_timer = timer_read();
    } else if (!edge &&
               timer_elapsed(fsr_hid_stream_timer) < FSR_DEBUG_INTERVAL_MS) {
        return;
    }
    fsr_hid_stream_timer = timer_read();
    int16_t witness = 0;
#    ifdef FSR_WITNESS_PIN
    witness = fsr_witness_reading;
#    endif
    fsr_sentinel_hid_send_sample(
        fsr_scan_count, fsr_scan_time_ms, reading, fsr_filtered,
        fsr_saturate_i16(fsr_hid_motion_x), fsr_saturate_i16(fsr_hid_motion_y),
        witness, fsr_touched ? 1U : 0U, (uint8_t)fsr_sentinel_output.state,
        fsr_sentinel_runtime_active_algorithm(),
        fsr_sentinel_runtime_generation());
    fsr_hid_motion_x = 0;
    fsr_hid_motion_y = 0;
}

static void fsr_record_motion(report_mouse_t report) {
    if (!fsr_active) {
        return;
    }
    fsr_debug_motion_x += report.x;
    fsr_debug_motion_y += report.y;
    fsr_hid_motion_x += report.x;
    fsr_hid_motion_y += report.y;
    fsr_sentinel_motion_x += report.x;
    fsr_sentinel_motion_y += report.y;
}

static void fsr_step_sentinel(int16_t reading) {
    uint32_t sentinel_elapsed_ms =
        fsr_scan_time_ms - fsr_sentinel_last_step_ms;
    if (fsr_sentinel_generation != fsr_sentinel_runtime_generation()) {
        dprintf("FSR SENTINEL SWITCH scan:%lu t_ms:%lu algorithm:%u "
                "generation:%u; forcing release\n",
                (unsigned long)fsr_scan_count,
                (unsigned long)fsr_scan_time_ms,
                fsr_sentinel_runtime_active_algorithm(),
                fsr_sentinel_runtime_generation());
        fsr_reset_sentinel("switch");
        sentinel_elapsed_ms = 0;
    }
    uint16_t sentinel_interval_ms = fsr_sentinel_runtime_interval_ms(
        &fsr_sentinel, FSR_SENTINEL_SHADOW_INTERVAL_MS);
    if (!fsr_sentinel_has_stepped ||
        sentinel_elapsed_ms >= sentinel_interval_ms) {
        fsr_sentinel_last_gap =
            fsr_sentinel_has_stepped &&
            sentinel_elapsed_ms >= FSR_SENTINEL_SHADOW_GAP_MS;
        fsr_sentinel_scan += fsr_sentinel_last_gap ? 2U : 1U;
        fsr_sentinel_input = (uint16_t)reading;
        int16_t motion_x = fsr_sentinel_motion_x < INT16_MIN
                               ? INT16_MIN
                               : fsr_sentinel_motion_x > INT16_MAX
                                     ? INT16_MAX
                                     : (int16_t)fsr_sentinel_motion_x;
        int16_t motion_y = fsr_sentinel_motion_y < INT16_MIN
                               ? INT16_MIN
                               : fsr_sentinel_motion_y > INT16_MAX
                                     ? INT16_MAX
                                     : (int16_t)fsr_sentinel_motion_y;
        fsr_sentinel_output_t next =
            fsr_sentinel_runtime_step(&fsr_sentinel, fsr_sentinel_input,
                                      fsr_scan_time_ms, fsr_sentinel_scan,
                                      motion_x, motion_y);
        fsr_sentinel_motion_x = 0;
        fsr_sentinel_motion_y = 0;
        bool touch_changed    = next.touched != fsr_touched;
        fsr_touched           = next.touched;
        if (touch_changed) {
            fsr_touch_timer = fsr_touched ? fsr_scan_time_ms : 0;
        }
        fsr_sentinel_output       = next;
        fsr_filtered =
            (int16_t)fsr_sentinel_runtime_filtered(&fsr_sentinel);
        fsr_sentinel_last_step_ms = fsr_scan_time_ms;
        fsr_sentinel_has_stepped  = true;
    }
}

bool is_fsr_touched(void) {
    return fsr_active && fsr_touched;
}

#    if defined(POINTING_DEVICE_ENABLE)
report_mouse_t pointing_device_task_fsr_layer(report_mouse_t mouse_report) {
    fsr_record_motion(mouse_report);
    return pointing_device_task_fsr_layer_kb(mouse_report);
}
#    endif

#    if defined(FSR_MOUSE_LAYER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
bool auto_mouse_activation(report_mouse_t mouse_report) {
    (void)mouse_report;
    return is_fsr_touched();
}
#    endif

void keyboard_post_init_fsr_layer(void) {
    fsr_sentinel_nvm_load();
    fsr_reset_sentinel("connect");
#    if defined(FSR_MOUSE_LAYER) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
    set_auto_mouse_layer(FSR_MOUSE_LAYER);
    set_auto_mouse_enable(true);
#    endif
    dprintf("FSR enabled scan:%lu t_ms:%lu pin:%d mouse_layer:%d "
            "auto_mouse_bridge:%d side:%s scan_interval_ms:%d "
            "sentinel_interval_ms:%d algorithm:%u\n",
            (unsigned long)fsr_scan_count, (unsigned long)timer_read32(),
            FSR_PIN, fsr_mouse_layer(), fsr_auto_mouse_bridge_enabled(),
            FSR_ON_LEFT_SIDE ? "left" : "right", FSR_SCAN_INTERVAL_MS,
            FSR_SENTINEL_SHADOW_INTERVAL_MS,
            fsr_sentinel_runtime_active_algorithm());
    fsr_log_status("connect");
}

#endif // FSR_ENABLE

#ifndef FSR_ENABLE
bool is_fsr_touched(void) {
    return false;
}
#endif

bool process_record_fsr_layer(uint16_t keycode, keyrecord_t *record) {
#ifdef FSR_ENABLE
    if (!fsr_is_sensor_side()) {
        return true;
    }

    switch (keycode) {
        case FSR_CAL:
            if (record->event.pressed) {
                fsr_request_manual_reset();
            }
            return false;
        case FSR_TOG:
            if (record->event.pressed) {
                fsr_active               = !fsr_active;
                fsr_manual_reset_pending = false;
                fsr_initialized          = false;
                fsr_touched              = false;
                fsr_touch_timer          = 0;
                fsr_reset_sentinel(fsr_active ? "enable" : "disable");
                dprintf("FSR scan:%lu t_ms:%lu %s\n",
                        (unsigned long)fsr_scan_count,
                        (unsigned long)timer_read32(),
                        fsr_active ? "enabled" : "disabled");
            }
            return false;
    }
#else
    (void)keycode;
    (void)record;
#endif
    return true;
}

void housekeeping_task_fsr_layer(void) {
#ifdef FSR_ENABLE
    fsr_sentinel_nvm_task();

    if (!fsr_is_sensor_side()) {
        return;
    }

    if (!fsr_active) {
        fsr_touched = false;
        return;
    }

    if (fsr_scan_timer != 0 &&
        timer_elapsed(fsr_scan_timer) < FSR_SCAN_INTERVAL_MS) {
        return;
    }
    fsr_scan_timer = timer_read();

    int16_t reading = analogReadPin(FSR_PIN);
#    ifdef FSR_WITNESS_PIN
    /* Discard one conversion after the GP26 read so channel settling cannot
     * leave a ghost mid-scale value on an open witness pin. */
    (void)analogReadPin(FSR_WITNESS_PIN);
    fsr_witness_reading = analogReadPin(FSR_WITNESS_PIN);
    fsr_witness_touched =
        fsr_witness_reading >= FSR_WITNESS_TOUCH_THRESHOLD;
    fsr_adc_probe_26 = reading;
    fsr_adc_probe_28 = analogReadPin(GP28);
    fsr_adc_probe_29 = analogReadPin(GP29);
#    endif
    fsr_scan_count++;
    fsr_scan_time_ms = timer_read32();
    fsr_last_reading = reading;

    if (fsr_manual_reset_pending) {
        fsr_manual_reset_pending = false;
        fsr_reset_sentinel("manual");
        fsr_last_logged_touched = fsr_touched;
    } else if (!fsr_initialized) {
        fsr_reset_sentinel("connect");
        fsr_last_logged_touched = fsr_touched;
    }

    fsr_step_sentinel(reading);

#    if FSR_TOUCH_TIMEOUT_SECONDS > 0
    if (fsr_touched && fsr_touch_timer != 0) {
        uint32_t timeout_ms = (uint32_t)FSR_TOUCH_TIMEOUT_SECONDS * 1000UL;
        if (timer_elapsed32(fsr_touch_timer) >= timeout_ms) {
            dprintf("FSR WARNING scan:%lu t_ms:%lu touch_timeout:%ds; "
                    "resetting detector and forcing release\n",
                    (unsigned long)fsr_scan_count,
                    (unsigned long)fsr_scan_time_ms,
                    FSR_TOUCH_TIMEOUT_SECONDS);
            fsr_reset_sentinel("timeout");
            fsr_last_logged_touched = fsr_touched;
        }
    }
#    endif

#    ifdef FSR_DEBUG_COMPACT
    fsr_debug_log_compact(reading);
#    else
    fsr_debug_buffer_sample(reading);
    if (fsr_debug_timer == 0) {
        fsr_debug_timer = timer_read();
    } else if (timer_elapsed(fsr_debug_timer) >= FSR_DEBUG_INTERVAL_MS ||
               fsr_debug_buf_count >= FSR_DEBUG_BUFFER_SAMPLES) {
        fsr_debug_timer = timer_read();
        fsr_debug_flush_batch();
    }
#    endif
    fsr_hid_maybe_send(reading);
#else
    /* Module linked but inactive until FSR_ENABLE is defined. */
#endif
}
