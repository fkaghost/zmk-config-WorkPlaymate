# Play/Work nice!view for the left-side Corne

This package targets the repository:

`typeractivexyz/corne-wireless-view-zmk-config`

and its current ZMK `v0.3` manifest.

## What the display shows

### Top block
- Battery percentage
- `USB` when USB is selected
- `BT1 ON` / `BT2 ON` when connected
- `BT1 OFF` / `BT2 OFF` when paired but disconnected
- `PAIRING` when the selected Bluetooth profile has no bond

### Middle block
- Layer 0: `PLAY`
- Layer 1: `WORK`
- Layer 2: `WORK 2`

### Bottom block
Only Bluetooth profiles 1 and 2 are displayed.

- Solid outer circle: connected
- Dashed outer circle: paired/bonded but disconnected
- No outer circle: empty/unbound
- Filled center: currently selected profile

## Install

Copy these paths into the root of your ZMK config repository:

- `zephyr/module.yml`
- `boards/shields/playwork_view/`
- `build.yaml`

The included `build.yaml` intentionally builds only the left Corne half:

```yaml
include:
  - board: nice_nano_v2
    shield: corne_left nice_view_adapter playwork_view
    snippet: studio-rpc-usb-uart
```

Do **not** also include the stock `nice_view` shield on the same build entry.
`playwork_view` replaces it.

Your normal `config/corne.keymap` and `config/corne.conf` stay where they are.

Push the files to GitHub. The existing ZMK GitHub Action should build a new
left-half UF2 firmware.

## Notes

The module reuses the stock ZMK v0.3 nice!view canvas utilities from the ZMK
source tree, rather than copying them. That keeps the custom code small and
preserves the stock display rotation behavior.

If you later move the repository from ZMK `v0.3` to `main`, the LVGL display
API changes and this widget should be updated at the same time.
