# FSR synchronized logging guide

## What the firmware records

The module samples the main GP26 FSR every `FSR_SCAN_INTERVAL_MS` (1 ms on
ddyo).

With `FSR_DEBUG_COMPACT` (ddyo keymap), every scan prints one short CSV line
immediately (no RAM batching):

```text
scan,t_ms,r,touch,mx,my,p28
```

| Field | Meaning |
|---|---|
| `scan` | Module sample counter |
| `t_ms` | QMK `timer_read32()` at this ADC sample |
| `r` | Raw GP26 ADC |
| `touch` | Sentinel touched (`0`/`1`) |
| `mx`,`my` | Trackball delta since previous sample |
| `p28` | GP28 witness ADC |

Without `FSR_DEBUG_COMPACT`, samples go into a RAM backlog and every
`FSR_DEBUG_INTERVAL_MS` the backlog is printed as full `FSR SCAN` lines.

Each FSR row includes:

- `scan` — module sample counter (ordering / gap detection)
- `t_ms` — QMK `timer_read32()` at the ADC scan (firmware time, not host)
- `host_ms` — added later by host capture tools when the Mac receives the line

USB and console buffering can deliver several lines together. Therefore
`host_ms` can repeat or arrive in bursts. It is not the ADC acquisition time.
Use `scan` and `t_ms` for detector timing.

Non-compact row shape:

```text
FSR SCAN scan:  114076 t_ms: 2285907 r:1987 f:1987 touch:0 state:idle     m:   0,   0 thumb:1240/1@29 p26:1987 p28:  12 p29: 876 alg:1 gen:0
```

| Field | Meaning |
|---|---|
| `r` / `f` | Raw and median-filtered GP26 ADC |
| `touch` / `state` | Active Sentinel decision |
| `m:x,y` | Trackball delta since the previous 1 ms sample |
| `thumb:reading/flag@pin` | Witness ADC, threshold flag, and GPIO number |
| `p26` / `p28` / `p29` | Probe ADC channels |
| `alg` / `gen` | Active algorithm ID and runtime reset generation |

If the RAM buffer fills before a flush, a `FSR DEBUG drop:N` warning is printed.

## Thumb witness FSR

The optional thumb FSR is off unless the keymap defines `FSR_WITNESS_PIN`
(ddyo: enable with `#define FSR_WITNESS_ENABLE`, which sets `FSR_WITNESS_PIN` to
`GP28`). When enabled it is logged only and never drives touch or Auto Mouse:

```c
#define FSR_WITNESS_ENABLE
/* or directly: */
#define FSR_WITNESS_PIN GP28
#define FSR_WITNESS_TOUCH_THRESHOLD 100
```

With the witness enabled, GP26 is read first and the thumb pin immediately
afterward. Both values are attached to the same module `scan` and `t_ms`,
although the ADC conversions are sequential, not simultaneous.

`thumb` is `reading/touched@pin`. Touched is `1` when
`reading >= FSR_WITNESS_TOUCH_THRESHOLD`. Set the threshold in the keymap after
observing unloaded and touched values.

The raw reading is the evidence. The `/0|/1` flag is only a convenient capture
label with no hysteresis. The thumb sensor never affects the main detector,
`is_fsr_touched()`, Auto Mouse, or the pointer layer.

When the witness is disabled, compact CSV omits the `p28` column
(`scan,t_ms,r,touch,mx,my`) and GP28 is free for other uses.

The assumed rising-value divider is:

```text
3.3 V -> thumb FSR -> thumb ADC pin -> fixed resistor -> GND
```

Do not feed an RP2040 ADC pin with 5 V. If the raw reading falls instead of
rises on touch, the threshold comparison does not describe that wiring.

With only the pulldown installed (no FSR), `thumb` should sit near 0. A stable
mid-scale value on an open pin usually means the pad is still tied to other
copper, the wrong pad is probed, or ADC channel settling needs a second read.

## Native QMK pointer debug

The ddyo keymap may define `POINTING_DEVICE_DEBUG`. With QMK debug output
enabled, the PMW3360 driver emits native rows such as its motion flag and raw
`dx`/`dy`. These rows are independent of `FSR_DEBUG_INTERVAL_MS` and may be much
more frequent than FSR rows.

The FSR row also contains `m:x,y`. These are signed pointer deltas accumulated
since the previous FSR row, then reset after printing. V3 has a separate
accumulator reset at each 20 ms detector step, so printing cannot consume its
input. Use native PMW3360 rows for lower-level sensor inspection.

Native pointer debug can create heavy console traffic and make host delivery
more bursty. Prefer leaving `POINTING_DEVICE_DEBUG` disabled and using `m:x,y`.

## Capture in tmux

Run the capture from this userspace repository. These commands are compatible
with Nushell:

```nu
cd /Users/dan/Projects/keebs/qmk_userspace
tmux new-session -A -s QMK
mise run capture-fsr
```

Use this sequence:

1. Wait for `QMK Console connected`.
2. Before pressing Enter in the capture program, press the keymap's `DB_TOGG`
   key until FSR debug lines appear. Pre-start output is not kept in the fresh
   capture.
3. Start the screen/video recording with the terminal and desired audio tracks
   visible or audible.
4. Press Enter in the capture program. It creates fresh files, prints the start
   sync marker, and plays three start tones.
5. Perform the touches, releases, typing, and trackball movements.
6. Press Ctrl-C once. Keep recording until the two end tones finish and the
   program prints the saved capture directory.

A second Ctrl-C force-stops the wrapper and leaves an incomplete capture for
inspection.

Detach without stopping the capture with `Ctrl-b`, then `d`. Reattach later:

```nu
tmux attach-session -t QMK
```

List sessions or save the current pane history manually:

```nu
tmux list-sessions
tmux capture-pane -p -S - -t QMK | save --force qmk-pane.log
```

The pane dump is a fallback. The capture wrapper's own files are authoritative
and do not depend on tmux scrollback.

## Capture files and synchronization

`mise run capture-fsr` creates a new directory below
`fsr-recordings/<date>-video-run<n>/` containing:

- `console.timestamped.log`: fresh console lines, each prefixed with `host_ms`.
- `sync-events.csv`: machine-readable start/end and individual tone-launch
  timestamps.

The wrapper prints lines shaped like:

```text
host_ms:1785755202241 FSR SCAN scan:  114076 t_ms: 2285907 r:1987 f:1987 touch:0 state:idle     m:   0,   0 thumb:1240/1@29
```

`SYNC event:start` and `SYNC event:end` are the authoritative visible anchors
when the terminal is in the recording. `SYNC_BEEP` records when the host
launches each sound, not the exact acoustic onset at the microphone or speaker.
Audio hardware and process startup can add delay, so retain the visible marker
and audio as separate evidence.

For analysis:

1. Preserve original line order.
2. Parse `FSR` rows by `scan` and `t_ms`.
3. Use `host_ms` to join console rows to external recordings.
4. Keep raw thumb readings even if the configured threshold later proves poor.
5. Treat missing scan numbers as unlogged samples unless `t_ms` also indicates
   that firmware sampling stalled.
