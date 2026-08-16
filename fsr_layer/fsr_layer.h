// Copyright 2026 ddyo
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>

/**
 * FSR touch via the selectable Sentinel runtime, with an optional QMK Auto
 * Mouse bridge.
 *
 * The module samples the main FSR every FSR_SCAN_INTERVAL_MS and steps the
 * active Sentinel algorithm on its own cadence (v1–v3 use
 * FSR_SENTINEL_SHADOW_INTERVAL_MS; Atlas uses its editable cadence).
 * is_fsr_touched() always follows the selected Sentinel. Reboot selects Atlas.
 * Live algorithm and parameter changes are RAM-only via Raw HID / WebHID.
 *
 * When FSR_MOUSE_LAYER and POINTING_DEVICE_AUTO_MOUSE_ENABLE are defined,
 * is_fsr_touched() is the Auto Mouse activation signal.
 *
 * Keycodes:
 *     FSR_CAL / FSRCAL  - reset the detector and force release
 *     FSR_TOG / FSRTOG  - toggle FSR scanning
 */

#ifndef FSR_PIN
#    define FSR_PIN GP26
#endif

/*
 * Optional independent witness FSR (enable with FSR_WITNESS_PIN / keymap
 * FSR_WITNESS_ENABLE). Its thresholded state is logged only and never
 * participates in touch detection or Auto Mouse activation.
 */
#if defined(FSR_WITNESS_PIN) && !defined(FSR_WITNESS_TOUCH_THRESHOLD)
#    define FSR_WITNESS_TOUCH_THRESHOLD 100
#endif

#ifndef FSR_SCAN_INTERVAL_MS
#    define FSR_SCAN_INTERVAL_MS 1
#endif

/*
 * Compact console rows for high-rate capture. When defined, every scan prints
 * immediately (no RAM batching). Format (CSV, no header):
 *
 *   scan,t_ms,r,touch,mx,my,p28
 *
 *   scan  - module sample counter
 *   t_ms  - QMK timer_read32() at this ADC sample
 *   r     - raw GP26 ADC
 *   touch - Sentinel touched (0/1)
 *   mx,my - trackball delta since previous sample
 *   p28   - GP28 witness ADC (only when FSR_WITNESS_PIN is set)
 *
 * USB console budget is ~32 B/ms; keep lines short for ~1 kHz.
 */
/* #define FSR_DEBUG_COMPACT */

/*
 * Console flush cadence for the 1 ms sample backlog (non-compact mode only).
 * Samples are collected every FSR_SCAN_INTERVAL_MS into a RAM buffer and dumped
 * as separate FSR SCAN lines every FSR_DEBUG_INTERVAL_MS.
 */
#ifndef FSR_DEBUG_INTERVAL_MS
#    define FSR_DEBUG_INTERVAL_MS 20
#endif

/* Capacity for the debug sample backlog (scan cadence × flush window + slack). */
#ifndef FSR_DEBUG_BUFFER_SAMPLES
#    define FSR_DEBUG_BUFFER_SAMPLES \
        ((FSR_DEBUG_INTERVAL_MS / FSR_SCAN_INTERVAL_MS) + 4)
#endif

#ifdef FSR_DEBUG_COMPACT
#    define FSR_DEBUG_COMPACT_ENABLED 1
#else
#    define FSR_DEBUG_COMPACT_ENABLED 0
#endif

/*
 * Default Sentinel cadence for v1–v3. Atlas uses its own editable cadence.
 * Frozen run7-provisional-v1 was validated on ~20 ms observations.
 */
#ifndef FSR_SENTINEL_SHADOW_INTERVAL_MS
#    define FSR_SENTINEL_SHADOW_INTERVAL_MS 20
#endif

/* V1 uses this pause to create a scan gap and clear CUSUM evidence. */
#ifndef FSR_SENTINEL_SHADOW_GAP_MS
#    define FSR_SENTINEL_SHADOW_GAP_MS 40
#endif

/*
 * Emergency recovery for an indefinitely held logical touch. Set to 0 or lower
 * to disable. Keeping this recovery enabled is highly recommended.
 */
#ifndef FSR_TOUCH_TIMEOUT_SECONDS
#    define FSR_TOUCH_TIMEOUT_SECONDS 60
#endif

/* Which physical half the FSR is soldered to. */
#ifndef FSR_ON_LEFT_SIDE
#    define FSR_ON_LEFT_SIDE 0
#endif

#if FSR_SCAN_INTERVAL_MS < 1
#    error "FSR_SCAN_INTERVAL_MS must be at least 1"
#endif

#if FSR_DEBUG_INTERVAL_MS < 1
#    error "FSR_DEBUG_INTERVAL_MS must be at least 1"
#endif

#if FSR_DEBUG_BUFFER_SAMPLES < 2
#    error "FSR_DEBUG_BUFFER_SAMPLES must be at least 2"
#endif

#if FSR_SENTINEL_SHADOW_INTERVAL_MS < 1
#    error "FSR_SENTINEL_SHADOW_INTERVAL_MS must be at least 1"
#endif

#if FSR_SENTINEL_SHADOW_GAP_MS <= FSR_SENTINEL_SHADOW_INTERVAL_MS
#    error "FSR_SENTINEL_SHADOW_GAP_MS must exceed the shadow interval"
#endif

/* Confirmed sensor state for custom integrations when FSR_MOUSE_LAYER is absent. */
bool is_fsr_touched(void);
