# QMK modules

Community modules for [QMK Firmware](https://github.com/qmk/qmk_firmware).

## Modules

- [`ddyo/fsr_layer`](fsr_layer): analog force sensing, selectable touch
  detection, QMK Auto Mouse integration, WebHID configuration, and EEPROM
  persistence.
- [`ddyo/argos`](argos): a fork of
  [BastardKB's Argos module](https://github.com/Bastardkb/qmk_modules/tree/main/argos)
  for runtime keyboard configuration.

### Argos changes from upstream

- Optional external VIA command dispatch with
  `ARGOS_DISABLE_VIA_COMMAND_KB`.
- Tapping-term settings fall back to QMK defaults until explicitly set.
- Optional custom tapping-term handling with
  `ARGOS_DISABLE_GET_TAPPING_TERM`.

## Install

```sh
git submodule add https://github.com/dan-dr/qmk_modules.git modules/ddyo
git submodule update --init --recursive
```

Add modules to `keymap.json`:

```json
{
  "modules": [
    "ddyo/argos",
    "ddyo/fsr_layer"
  ]
}
```

## Tests

```sh
python3 -m unittest discover -s fsr_layer/tests -p 'test_*.py'
```

## License

GPL-2.0-or-later. Individual source files and module manifests carry their
copyright and license identifiers.
