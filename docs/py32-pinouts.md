# PY32F030 TSSOP-20 pinouts vs STM32

Extracted from the **PY32F030 Datasheet** (figures 3-12 … 3-15) and ST's KiCad
symbol for `STM32G030F6Px`, a common part on existing Jacdac modules. See also
[`py32-porting.md`](py32-porting.md).

> ⚠️ **PY32F030 has four different TSSOP-20 pinouts** (`F1`–`F4`). The digit
> after the package letter selects it: the part used here, **`PY32F030F28P6TU`, is the `F2`
> pinout** (`F` = TSSOP-20, `2` = pinout variant 2, `8` = 64 KB flash).

## The comparison that matters

| Pin | **STM32G030F6P6** | **PY32F030`F2`8P6TU** | match |
|---:|---|---|:--:|
| 1 | PB7/PB8 | PA2 | ✗ |
| 2 | PB9/PC14 | PA3 | ✗ |
| 3 | PC15 | PA4 | ✗ |
| 4 | **VDD** | PA5 | ✗ |
| 5 | **VSS** | PA6 | ✗ |
| 6 | NRST | PA7 | ✗ |
| 7 | PA0 | **VSS** | ✗ |
| 8 | PA1 | PA12 | ✗ |
| 9 | PA2 | **VCC** | ✗ |
| 10 | PA3 | PA13 | ✗ |
| 11 | PA4 | PA14 | ✗ |
| 12 | PA5 | PB5 | ✗ |
| 13 | PA6 | PB6 | ✗ |
| 14 | PA7 | PB7 | ✗ |
| 15 | PA8/PB0/PB1/PB2 | PF4-BOOT0 | ✗ |
| 16 | PA9/PA11 | PF0-OSCIN | ✗ |
| 17 | PA10/PA12 | PF1-OSCOUT | ✗ |
| 18 | PA13 | PF2-NRST | ✗ |
| 19 | PA14/PA15 | PA0 | ✗ |
| 20 | PB3/PB4/PB5/PB6 | PA1 | ✗ |

**Zero pins match — including power.** `VDD/VSS` land on pins 4/5 (G030) vs 9/7
(PY32). No firmware remap can fix a power-pin mismatch, so **a G030 → PY32
migration requires a board revision.** This is unlike the G030 → C031 swap,
which was pin-for-pin.

**No PY32 TSSOP-20 variant is G030-compatible** — power pins by variant:
`F1` VSS 7/VCC 9 · `F2` VSS 7/VCC 9 · `F3` VSS 15/VCC 16 · `F4` VSS 6/VCC 7.
None land on G030's VDD 4 / VSS 5.

## All four PY32F030 TSSOP-20 variants

| Pin | F1 (`F1xPx`) | **F2 (`F2xPx`)** ← ours | F3 (`F3xPx`) | F4 (`F4xP`) |
|---:|---|---|---|---|
| 1 | PF0-OSCIN | **PA2** | PF4-BOOT0 | PA4 |
| 2 | PF1-OSCOUT | **PA3** | PF0-OSC_IN | PA6 |
| 3 | PF2-NRST | **PA4** | PF1-OSC_OUT | PA7 |
| 4 | PF3 | **PA5** | PF2-NRST | PB0 |
| 5 | PA2 | **PA6** | PF3 | PB1 |
| 6 | PA3 | **PA7** | PA0 | VSS |
| 7 | VSS | **VSS** | PA1 | VCC |
| 8 | PB2 | **PA12** | PA2 | PA8 |
| 9 | VCC | **VCC** | PA3 | PA9 |
| 10 | PA8 | **PA13** | PA4 | PA10 |
| 11 | PA9-OSC32OUT | **PA14** | PA5 | PA13-SWD |
| 12 | PA10-OSC32IN | **PB5** | PA6 | PA14-SWC |
| 13 | PA11 | **PB6** | PA7 | PA5/PB4 |
| 14 | PA13 | **PB7** | PB1 | PF0-OSCIN |
| 15 | PA14 | **PF4-BOOT0** | VSS | PF1-OSCOUT |
| 16 | PB4 | **PF0-OSCIN** | VCC | PF2-NRST |
| 17 | PB5 | **PF1-OSCOUT** | PA9 | PA0/PF3 |
| 18 | PB6 | **PF2-NRST** | PA10 | PA1 |
| 19 | PB7 | **PA0** | PA13 | PA2 |
| 20 | PF4-BOOT0 | **PA1** | PA14 | PA3 |

## Notable: `F3` ≈ STM32F030F4P6

The `F3` variant is near pin-identical to **STM32F030F4P6** (only pin 5 differs:
`PF3` vs `VDDA`). This confirms Puya modelled the PY32 20-pin part on ST's
**F0** line, not the G0. Of limited practical use, since the STM32F030F4P6 has
only 4 KB RAM, but worth knowing if a footprint ever
needs ST/Puya dual-sourcing.

## Why `F2` is nonetheless the right variant

Measured against a typical STM32G030 Jacdac module pin map (bus on PB6, WS2812
on SPI1, RGB status LED on TIM3):

| Module need | F1 | **F2** | F3 | F4 |
|---|:--:|:--:|:--:|:--:|
| **PB6** — Jacdac bus UART | ✓ | **✓** | ✗ | ✗ |
| PA13 / PA14 — SWD | ✓ | **✓** | ✓ | ✓ |
| PA5 — SPI1 SCK | ✗ | **✓** | ✓ | ~ |
| PA12 — SPI1 MOSI (WS2812) | ✗ | **✓** | ✗ | ✗ |
| PA6 / PA7 — status LED | ✗ | **✓** | ✓ | ✓ |
| PB1 — status LED (blue) | ✗ | ✗ | ✓ | ✓ |

`F2` is the **only** variant carrying both **PB6** (the usual Jacdac UART pin on
STM32 modules) **and PA12**. The single miss is `PB1`, for which `PB5`
or `PB7` substitutes. So `PY32F030F28P6TU` is a good choice — just not a
drop-in.

> **Verified below:** PB6 is USART1_TX on AF0, as on STM32.

---

# Alternate-function mapping — PY32 vs STM32

From the PY32F030 datasheet §3.1–3.3 (pages 51–52), read from the rendered
tables (the text layer mis-aligns rows — trust the figures, not `pdftotext`).

## ✅ Headline: a typical STM32 module pin map transfers verbatim

PY32 mirrors ST's **AF0** assignments for SPI1 / USART1 / SWD. Every pin such
modules rely on maps to the *same function on the same AF*:

| Module need (`board.h`) | STM32G030 | **PY32F030** | |
|---|---|---|:--:|
| `UART_PIN PB_6`, `USART_IDX 1`, `AF0` — Jacdac bus | PB6 = USART1_TX @AF0 | **PB6 = USART1_TX @AF0** | ✅ |
| `PIN_ASCK PA_5` — SPI1 SCK | PA5 = SPI1_SCK @AF0 | **PA5 = SPI1_SCK @AF0** | ✅ |
| `PIN_AMOSI PA_12` — WS2812 data | PA12 = SPI1_MOSI @AF0 | **PA12 = SPI1_MOSI @AF0** | ✅ |
| SWD | PA13/PA14 | **PA13 = SWDIO, PA14 = SWCLK** | ✅ |
| `PIN_LED_R PA_6` | TIM3_CH1 | **PA6 @AF1 = TIM3_CH1** | ✅ |
| `PIN_LED_G PA_7` | TIM3_CH2 | **PA7 @AF1 = TIM3_CH2** | ✅ |
| `PIN_LED_B PB_1` | TIM3_CH4 | PB1 exists on die, **not bonded on `F2`** | ⚠️ |

Datasheet note 2 also confirms **PA13/PA14 come up as SWDIO/SWCLK after reset**,
so a blank part is debuggable out of the box.

## ⚠️ The one gap: blue status-LED channel

`PB1` (the usual blue LED PWM) is **not bonded on the `F2` pinout**. Alternatives
available on `F2` with a timer output:

| Pin | AF | Timer | Note |
|---|---|---|---|
| PA4 | AF4 | TIM14_CH1 | independent timer — cleanest |
| PA0 | AF13 | TIM1_CH3 | |
| PA1 | AF13 | TIM1_CH4 | |
| PB7 | AF2 | TIM17_CH1N | complementary output |

Red/green stay on TIM3_CH1/CH2 (PA6/PA7); blue moves to **PA4/TIM14_CH1** as the
tidiest option. A three-channel RGB PWM is achievable — just not on PB1.

## SPI1 / USART1 quick reference (AF0 unless noted)

| Function | PY32 pins | on `F2`? |
|---|---|:--:|
| USART1_TX | PB6, PA14@AF1, PA2@AF1, PA7@AF8, PB8@AF8, PF1@AF8 | ✅ PB6 |
| USART1_RX | PB7, PB2, PA3@AF1, PA13@AF8, PF0@AF8 | ✅ PB7 |
| SPI1_SCK | PA5, PB3, PA1, PA2@AF10 | ✅ PA5 |
| SPI1_MOSI | PA12, PA7, PB5, PA2, PA1@AF10, PA3@AF10 | ✅ PA12/PA7 |
| SPI1_MISO | PA6, PB4, PA0@AF10, PA13@AF10 | ✅ PA6 |
| SPI1_NSS | PA4, PB0, PA15 | ✅ PA4 |

Single-wire Jacdac only needs the **TX** pin (`HDSEL` makes it bidirectional),
so **PB6 alone is sufficient** for the bus.

## Dev board: PY32F030K28U6 (QFN-32, `K2` pinout)

*Added 2026-09-23 from the PY32F030 datasheet V2.8 (table 3-2; AF maps in tables
3-6/3-7).* The dev board carries the 32-pin `K28U6` rather than the TSSOP-20
`F28P6`. The memory is the same, and so is the firmware apart from pins.

| Signal | Pin | QFN-32 `K2` pin |
|---|---|---|
| Jacdac bus: USART1_TX @ AF0 | PB6 | 29 |
| Pot wiper (`-slider`) | PA0 | 6 |
| Pot high end (`-slider`) | PB7 | 30 |
| Status LED | PA4 (the board's LED4, active low) | 10 |
| SWDIO / SWCLK | PA13 / PA14 | 23 / 24 |
| NRST | PF2 | 4 |
| BOOT0: **low at reset to boot from flash** | PF4 | 31 |

All of PA0-PA15, PB0-PB8 and PF0-PF4 are bonded on this package.

### Timer AFs, confirmed from the AF maps

These are the entries in the firmware's PWM tables (`stm32/pwm.c`,
`bl/blpwm.c`), all checked against the datasheet rendering (not a text
extraction, which misaligned rows once before):

| Pin | Timer channel | AF |
|---|---|---|
| PA0 / PA1 | TIM1_CH3 / TIM1_CH4 | **AF13** (differs from ST) |
| PA4 | TIM14_CH1 | AF4 (also TIM3_CH3 @ AF13) |
| PA6 / PA7 | TIM3_CH1 / TIM3_CH2 | AF1 |
| **PB0 / PB1** | **TIM3_CH3 / TIM3_CH4** | **AF1, as on ST** |

PB1 is the usual blue status LED on G030 modules, so on packages that bond it (this
QFN-32, and the SOP-16 F003) the familiar pin works. **These are the F030's
maps**; check the F003 datasheet's own before an F003 board.

## PY32F003 SOP-16 (`W1` pinout): the 4 KB RAM part

*Added 2026-09-23 from the PY32F003 datasheet V2.7 (English), table 3-3 and AF
maps 3-6/3-7, read as rendered pages.* Covers `PY32F003W16S6TU` (32 KB / 4 KB)
and `PY32F003W18S6TU` (64 KB / 8 KB).

| Pin | Signal | Firmware use (`py32f003w16-button`) |
|---|---|---|
| 1 | VSS | |
| 2 | PA12 | |
| 3 | PA13 (SWDIO) | SWD |
| 4 | **PA14 (SWCLK) + PF1**, two pads on one pin | SWD. **Never drive PF1** |
| 5 | **PF2 (NRST) + PF0**, two pads on one pin | Reset. **Never drive PF0** |
| 6 | PA0 | button (also ADC_IN0) |
| 7 | PA1 | (PB7 isn't bonded; PA1 can take its usual jobs) |
| 8 | **PA2**: USART1_TX @ **AF1** | **Jacdac bus** (PB6 isn't bonded) |
| 9 | PA3 | |
| 10 | PA4 | (TIM14_CH1 @ AF4 available) |
| 11 | PA5 | |
| 12 | PA6: TIM3_CH1 @ AF1 | LED R |
| 13 | PA7: TIM3_CH2 @ AF1 | LED G |
| 14 | PB0: TIM3_CH3 @ AF1 | |
| 15 | PB1: TIM3_CH4 @ AF1 | LED B |
| 16 | VCC | |

The AF maps match the F030's for every pin in the firmware's PWM tables.

