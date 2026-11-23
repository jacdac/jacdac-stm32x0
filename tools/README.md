# tools

## Flashing a PY32 over SWD

```bash
test/build-check.sh py32f030k28-devboard-button          # build
tools/flash.sh py32f030k28-devboard-button --erase      # first flash of a blank part
```

`flash.sh` flashes `built/<target>/combined-*.hex` (bootloader + app) with
**pyOCD**. After the first flash, updates can go over the Jacdac bus as UF2.

It flashes through **the one ST-Link plugged in**, and stops if there are none
or several. A micro:bit on USB is a debug probe too (CMSIS-DAP), and without
this check pyOCD would pick whichever probe it found first. To choose a probe,
set `PY32_PROBE=<unique id from pyocd list>`. For `pyocd cmd`, pass
`-u <unique id>` yourself.

Why pyOCD: **STM32CubeProgrammer won't flash PY32 parts**, and **mainline
OpenOCD (including Homebrew's 0.12.0) has no PY32 flash driver**. pyOCD drives
an ST-Link and uses the flash algorithm from Puya's own CMSIS device pack.

### Setup

- **pyOCD** 0.45.1, installed with `pipx install pyocd` (2026-09-23).
- **Puya device pack**: `tools/Puya.PY32F0xx_DFP.1.2.6.pack`. It is
  **git-ignored**, a 6.9 MB vendor binary whose redistribution terms haven't been
  checked. It comes from the official SDK zip already vendored from (see
  `py32/VENDORED-PUYA.md`), at `Packs/MDK/Puya.PY32F0xx_DFP.1.2.6.pack`.
  SHA-256 `78653f46d88070d62357704105dbc0f40da9df838fcda6c37d8ada0b0b0250d2`.
  pyOCD lists `py32f030x4/x6/x7/x8` from it.
- **Probe:** an ST-Link. pyOCD needs reasonably recent ST-Link firmware; if it
  refuses the probe, update the firmware with ST's updater first.

### Wiring the probe

| ST-Link | PY32 |
|---|---|
| SWDIO | PA13 |
| SWCLK | PA14 |
| GND | GND |
| 3.3 V (sense) | VCC |

On the PY32F030K28U6 (QFN-32) those are pins 23 and 24. **PF4 is BOOT0
(pin 31)**; it must be low at reset for the part to boot from flash.
