# FSR Layer

A QMK Community Module that turns an analog force sensor into a touch signal
for QMK Auto Mouse.

![FSR Sentinel web configurator](assets/web-configurator.png)

## Features

- Four runtime-selectable detection algorithms
- Optional trackball-motion input
- QMK Auto Mouse integration
- WebHID configuration without rebuilding firmware
- EEPROM-backed algorithm and parameter settings
- Console logging and Raw HID diagnostics

## How it works

1. Samples an analog FSR input.
2. Runs the selected detector.
3. Exposes the result through `is_fsr_touched()`.
4. Optionally uses that result as the QMK Auto Mouse activation signal.

The module does not switch layers directly. QMK Auto Mouse owns layer
activation, release, and timeout behavior.

## Algorithms

| Algorithm | Summary |
|---|---|
| `v1_dual_cusum` | Tracks a moving idle center and accumulates separate touch and release evidence. |
| `v2_signal_envelope` | Uses an adaptive signal envelope with touch and release confirmation. |
| `v3_motion_envelope` | Adds trackball-motion assistance to the v2 signal envelope. |
| `atlas_phase_v1` | Experimental phase-based detector with filtering, motion modes, dwell, and rebound controls. |

- Algorithms can be changed at runtime.
- `atlas_phase_v1` is the default when no valid saved configuration exists.
- Changing an algorithm or parameter resets detector state and forces release.

## Web configurator

Source: [`tools/fsr-sentinel-web`](https://github.com/dan-dr/qmk_userspace/tree/main/tools/fsr-sentinel-web)

- Runs in Chrome or Edge through WebHID.
- Reads available algorithms and parameters from firmware.
- Applies changes immediately.
- Supports restoring compile-time defaults.
- Shows whether settings have been saved.

## EEPROM persistence

- Saves about 0.5 seconds after the last change.
- Stores the active algorithm and all algorithm parameter blocks.
- Restores saved settings on boot.
- Uses a schema version and CRC to reject incompatible or corrupt data.
- Stores settings on the physical keyboard half connected to the sensor.

## Setup

Add the module to `keymap.json`:

```json
{
  "modules": ["ddyo/fsr_layer"]
}
```

Enable it in `config.h`:

```c
#define FSR_ENABLE
#define FSR_PIN GP26
```

Optional Auto Mouse bridge:

```c
#define POINTING_DEVICE_AUTO_MOUSE_ENABLE
#define FSR_MOUSE_LAYER 1
```

Useful options:

| Option | Default | Purpose |
|---|---:|---|
| `FSR_SCAN_INTERVAL_MS` | `1` | Analog sampling interval |
| `FSR_SENTINEL_SHADOW_INTERVAL_MS` | `20` | v1-v3 detector interval |
| `FSR_TOUCH_TIMEOUT_SECONDS` | `60` | Recovery timeout for a stuck touch |
| `FSR_ON_LEFT_SIDE` | `0` | Selects the sensor half on split keyboards |

## Keycodes

- `FSR_CAL` / `FSRCAL`: reset the detector and force release
- `FSR_TOG` / `FSRTOG`: toggle FSR sampling

## Diagnostics

- Capture and console fields: [`LOGGING.md`](LOGGING.md)
- Host tests: `python3 -m unittest discover -s fsr_layer/tests -p 'test_*.py'`
