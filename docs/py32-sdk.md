# Puya PY32 SDK — what's in it, and how ST-compatible is it?

Background for the port; see [`py32-porting.md`](py32-porting.md) for the summary.

First inspected via the community mirror
[`IOsetting/py32f0-template`](https://github.com/IOsetting/py32f0-template).
**Re-checked on 2026-09-23 against Puya's official `PY32F0xx_Firmware_V1.5.0`,
which is now vendored** (see [`../py32/VENDORED-PUYA.md`](../py32/VENDORED-PUYA.md)).
The API conclusions below hold for the official release, with one exception:
the mirror's `py32f0xx_ll_flash.c` (with `LL_FLASH_Erase()`) **is not in the
official release**.

---

## ✅ Headline: Puya ships a full **LL** layer — and it's a fork of ST's

Official v1.5.0: **22 LL headers / 16 sources**, inside
`Drivers/PY32F0xx_HAL_Driver` next to the HAL, plus CMSIS device headers, BSP,
templates and IDE packs. (The mirror's `Libraries/PY32F0xx_LL_Driver` has 17
sources; the extra one is `py32f0xx_ll_flash.c`.)

The license header is the tell:

```
© Copyright (c) Puya Semiconductor Co.   All rights reserved.
© Copyright (c) 2016 STMicroelectronics. All rights reserved.
This software component is licensed by ST under BSD 3-Clause license
```

**Puya forked ST's own LL drivers.** That's why the API lines up — and it means
`jacdac-stm32x0`, which is built on ST's LL, is far more reusable than a
clean-room vendor SDK would allow.

> ⚠️ **Correction after the port compiled (2026-09-23).** The API compatibility
> below is about **function and constant names**. The **peripherals** behind them
> are not all ST's modern designs, and that only showed once code touched
> registers:
>
> - **USART** is the **STM32F1** design: `SR/DR`, no `ISR`/`ICR`/`RDR`/`TDR`/`BUSY`/
>   `REACK`/`TEACK`, errors clear by reading `SR` then `DR`, clearing `UE` does
>   **not** reset the flags, and `OVER8` is in **`CR3`**.
> - **RTC** is the F1-style counter RTC, not the calendar RTC.
> - **ADC** is F0-style but has no `ADRDY` or `ADDIS`.
> - **EXTI**'s port selector covers only lines 0-8, with a different encoding.
>
> The LL makes these *compile*; making them *correct* needed real PY32 branches.
> The full list, and the silent bugs they would have caused, is in
> [`py32-porting.md`](py32-porting.md). The "Three deltas" section below understates the port.

**License: BSD-3-Clause** — permissive, MIT-compatible, vendorable with
attribution.

### Available LL modules
`adc · bus · comp · cortex · crc · dma · exti · flash · gpio · i2c · iwdg · led ·
lptim · pwr · rcc · rtc · spi · system · tim · usart · utils · wwdg`

That covers every module `jacdac-stm32x0` uses.

---

## API compatibility

### Works unchanged ✅
`LL_FLASH_SetLatency` · `LL_RCC_PLL_Enable` · `LL_RCC_SetSysClkSource` ·
`LL_RCC_HSI_Enable` · `LL_GPIO_SetPinMode` · `LL_GPIO_SetAFPin_0_7` ·
`LL_SPI_Enable` · `LL_DMA_ConfigTransfer` · `LL_DMA_EnableChannel` ·
`LL_USART_Enable` · `LL_TIM_EnableCounter` — plus the
`__HAL_RCC_<periph>_CLK_ENABLE()` macros in `py32f0xx_hal_rcc.h`.

Bus clock grouping is the **F0/C0 style** we already handle:
`LL_APB1_GRP1_EnableClock` · `LL_APB1_GRP2_EnableClock` ·
`LL_AHB1_GRP1_EnableClock` · `LL_IOP_GRP1_EnableClock`.

### ⭐ Confirms the DMA trap answer
```c
#define LL_DMA_CHANNEL_1   0x00000001U   /* 1-based — like F0/C0, not G0 */
```
Matches the `(ch-1)*4` ISR/IFCR shift derived from the registers
(see [`py32-clock-dma.md`](py32-clock-dma.md)). **Pre-answered before it could
bite us a second time.**

### ⚠️ Three deltas needing adaptation

| # | Area | ST / `jacdac-stm32x0` | Puya PY32 |
|---|---|---|---|
| 1 | **EXTI naming** | `LL_EXTI_EnableIT_0_31()`, `LL_EXTI_ClearFlag_0_31()` | `LL_EXTI_EnableIT()`, `LL_EXTI_ClearFlag()` — **no `_0_31` suffix** |
| 2 | **EXTI style** | G0/C0 use `RPR1`/`FPR1` (split rising/falling pending) | **single `EXTI->PR`** — i.e. the **F0 style**, so our F0 branch applies |
| 3 | **Half-duplex** | `LL_USART_ConfigHalfDuplexMode()` | `LL_USART_EnableHalfDuplex()` (sets `USART_CR3_HDSEL`) |
| 4 | **DMA request routing** | `LL_DMA_SetPeriphRequest(DMA1, ch, LL_DMAMUX_REQ_x)` | no DMAMUX — `LL_SYSCFG_DMA_MAP_*` constants in `ll_system.h` writing `SYSCFG_CFGR3` |

All four are **mechanical renames or one small new function** — not redesigns.

---

## Implication for the port

Because the deltas are naming-level rather than structural, a thin
**`py32_compat.h` shim** mapping ST names onto Puya's would let most of
`jacdac-stm32x0` compile nearly unchanged, e.g.:

```c
#define LL_EXTI_EnableIT_0_31(l)        LL_EXTI_EnableIT(l)
#define LL_EXTI_ClearFlag_0_31(l)       LL_EXTI_ClearFlag(l)
#define LL_USART_ConfigHalfDuplexMode(u) LL_USART_EnableHalfDuplex(u)
```
plus a small `dma_set_request(ch, req)` helper writing `SYSCFG_CFGR3` in place of
`LL_DMA_SetPeriphRequest()`.

This makes the port look much more like **"add a family"** than "write a
platform layer", which is how it was done: PY32 is a family in this repo.

---

## Sourcing note

The community template used for this inspection has **no LICENSE file at its
root**, even though the vendored driver files carry BSD-3-Clause headers. For
actual vendoring, take the SDK from **Puya's official release** rather than a
community mirror, and preserve the BSD-3-Clause + ST/Puya attribution.
**Done 2026-09-23:** vendored from puyasemi.com, not py32.org (which is the
community OpenPuya site). It's a copy, not a submodule, because Puya publishes
zips, not a git repository.
