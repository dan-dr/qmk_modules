# Argos

Fork of the
[BastardKB Argos module](https://github.com/Bastardkb/qmk_modules/tree/main/argos).

## Changes from upstream

- `ARGOS_DISABLE_VIA_COMMAND_KB` lets a keymap or another module own
  `via_command_kb()` and forward commands to `argos_handle_command()`.
- An unset tapping term uses QMK's dynamic or compile-time default.
- `ARGOS_DISABLE_GET_TAPPING_TERM` lets a keymap provide custom per-key tapping
  terms.

## Setup

Add the module to `keymap.json`:

```json
{
  "modules": ["ddyo/argos"]
}
```

For external VIA command dispatch:

```c
#define ARGOS_DISABLE_VIA_COMMAND_KB
```

Then forward unhandled VIA commands to `argos_handle_command()`.

For custom tapping-term logic:

```c
#define ARGOS_DISABLE_GET_TAPPING_TERM
```
