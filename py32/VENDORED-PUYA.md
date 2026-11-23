# Vendored Puya SDK files

A trimmed copy of Puya's official PY32F0 firmware package. The files are
**unmodified**: kept byte-for-byte, CRLF line endings included, so a future
Puya release can be diffed against them directly.

| | |
|---|---|
| Package | `PY32F0xx_Firmware_V1.5.0.zip` |
| Source | puyasemi.com, download page (`download.html?keywords=PY32F030`), "PY32F030 Firmware Library" |
| Published | 2026-08-18 (file dated 2026-07-31) |
| Size | 70,924,483 bytes |
| SHA-256 | `aae26b1470d09dbcf2d5992f594f73c9366639e49d842bab7f6db42856568d97` |
| Vendored | 2026-09-23 |

## What was taken, and from where in the package

| Here | Package path | Notes |
|---|---|---|
| `py32f0xx_ll_driver/Inc/py32f0xx_ll_*.h` (22) | `Drivers/PY32F0xx_HAL_Driver/Inc/` | LL only; the HAL is not vendored |
| `py32f0xx_ll_driver/Src/py32f0xx_ll_*.c` (16) | `Drivers/PY32F0xx_HAL_Driver/Src/` | there is no `py32f0xx_ll_flash.c`; flash LL is header-only (see below) |
| `py32f0xx_ll_driver/Inc/py32_assert.h` | `Templates/PY32F030xx_Templates_LL/Inc/` | per-project template; identical across F030/F003/F002 |
| `cmsis_device_py32f0/Include/` | `Drivers/CMSIS/Device/PY32F0xx/Include/` | only `py32f030x8` (F030 64 KB), `py32f003x6` / `x8` (F003 W16 / W18), `py32f002ax5` (F002A), plus `py32f0xx.h` and `system_py32f0xx.h` |
| `cmsis_device_py32f0/Source/system_py32f0xx.c` | `Drivers/CMSIS/Device/PY32F0xx/Source/` | |
| `cmsis_device_py32f0/Source/gcc/startup_py32f0{30,03,02}xx.s` | `Templates/PY32F0{30,03,02}xx_Templates_LL/EIDE/` | the EIDE (GCC) startups; `Source/arm/` is Keil syntax |
| `cmsis_core_py32/Include/` | `Drivers/CMSIS/Include/` | ARM CMSIS core **V4.30**: `core_cm0plus.h`, `core_cmInstr.h`, `core_cmFunc.h`, `cmsis_gcc.h` |

**Left out:** the HAL, BSP, examples, templates, IDE packs, the DSP/RTOS parts
of CMSIS, and device headers for memory variants we don't build.

## Flash: official release vs the community mirror

The **official v1.5.0 release has no `py32f0xx_ll_flash.c`**, and so no
`LL_FLASH_Erase()`. Flash support is inline functions and macros in
`py32f0xx_ll_flash.h`: `LL_FLASH_Unlock()`, `LL_FLASH_Lock()`,
`LL_FLASH_PageProgram()`, the erase-mode enables, and the **factory timing
calibration**:

```c
LL_FLASH_TIMMING_SEQUENCE_CONFIG_4M()    // one macro per HSI setting; each loads
LL_FLASH_TIMMING_SEQUENCE_CONFIG_8M()    // TS0/TS1/TS2P/TPS3/PERTPE/SMERTPE/
LL_FLASH_TIMMING_SEQUENCE_CONFIG_16M()   // PRGTPE/PRETPE from the factory info
LL_FLASH_TIMMING_SEQUENCE_CONFIG_22P12M()// block at 0x1FFF0F1C onward
LL_FLASH_TIMMING_SEQUENCE_CONFIG_24M()   // <- ours: HSI 24 MHz
```

(Puya's spelling, "TIMMING".) Puya's HAL does the same thing automatically:
`__HAL_FLASH_TIMMING_SEQUENCE_CONFIG()` reads `RCC_ICSCR.HSI_FS` and indexes a
table of the same addresses. We don't vendor the HAL, and we don't need it.

The community mirror `IOsetting/py32f0-template`, which was read for
`docs/py32-sdk.md`, **does** carry a `py32f0xx_ll_flash.c` with
`LL_FLASH_Erase()` (17 LL sources against the official 16). Don't write code
against that API; it isn't in the release we vendor.

## Licences

- **Puya LL drivers, device headers, startup files:** BSD-3-Clause, carrying
  both `© 2023 Puya Semiconductor Co.` and `© 2016 STMicroelectronics`. Puya's
  LL is a fork of ST's.
- **CMSIS core V4.30:** ARM's BSD-style licence, `© 2009–2015 ARM LIMITED`.

Both are compatible with this repo's MIT licence. The notices are in each file
header and must be preserved.

## Upstream (ST) equivalents

These sit alongside the ST families' folder names under `stm32/` and don't
collide with them. `cmsis_core_py32` is deliberately separate from
upstream's `cmsis_core` submodule path, which was dropped from this repo.
