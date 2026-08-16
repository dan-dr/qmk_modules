# ddyo QMK modules

Community modules for [QMK Firmware](https://github.com/qmk/qmk_firmware).
The repository follows QMK's official
[Community Modules](https://docs.qmk.fm/features/community_modules) layout.

## Modules

- [`ddyo/fsr_layer`](fsr_layer): FSR touch detection, QMK Auto Mouse bridging,
  selectable Sentinel detectors, Raw HID configuration, and EEPROM persistence.

## Add to an external userspace

```sh
git submodule add https://github.com/dan-dr/qmk_modules.git modules/ddyo
git submodule update --init --recursive
```

Then add the module to the keymap's `keymap.json`:

```json
{
  "modules": ["ddyo/fsr_layer"]
}
```

## Host tests

The portable tests run directly from this repository:

```sh
python3 -m unittest discover -s fsr_layer/tests -p 'test_*.py'
```

Some parity checks use ignored recordings and frozen reference models from
Dan's sibling `qmk_userspace` checkout. Set `QMK_USERSPACE` when it cannot be
discovered automatically:

```sh
QMK_USERSPACE=/path/to/qmk_userspace \
  python3 -m unittest discover -s fsr_layer/tests -p 'test_*.py'
```

## License

GPL-2.0-or-later. Source files and `qmk_module.json` carry the applicable
copyright and license identifiers.
