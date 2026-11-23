# PY32F030 clock tree & DMA

From the **PY32F030 Reference Manual V1.7** (517 pp) and datasheet. Background for
the port; see [`py32-porting.md`](py32-porting.md) for the summary.

---

## 1. Clock tree — a *third* pattern

PY32 is neither the G0 nor the C0 arrangement:

| | STM32G030 | STM32C031 | **PY32F030** |
|---|---|---|---|
| PLL | yes | **none** | **yes, fixed ×2** |
| Path to 48 MHz | HSI16 → PLL | HSI48 direct (`HSIDIV=1`) | **HSI 24 MHz → PLL ×2** |
| Reset default SYSCLK | HSI16 | HSI48/4 = 12 MHz | **HSI 8 MHz** |
| Flash latency @48 MHz | `LATENCY_2` (2-bit field) | `LATENCY_1` | **`LATENCY=1` (single bit)** |

### Details
- **HSI is configurable: 4 / 8 / 16 / 22.12 / 24 MHz.** Reset default is **8 MHz**.
- **PLL: fixed ×2**, source selectable HSI or HSE (`RCC_PLLCFGR.PLLSRC`:
  0 = HSI, 1 = HSE). There is **no multiplier field** anywhere in the RM — the
  datasheet's "supports 2 octaves" is a translation of ×2.
  PLL config registers **cannot be changed once the PLL is enabled**.
- **HSE:** 4–32 MHz, with CSS (on CSS failure hardware falls back to HSI and
  raises NMI; PLL+HSE are turned off).
- **LSI / LSE:** 32.768 kHz.
- **Flash:** `FLASH_ACR.LATENCY` is a **single bit** — `0` for system clock
  ≤ 24 MHz, **`1` above 24 MHz** (two clock cycles per read).
- AHB and APB dividers off SYSCLK; both up to 48 MHz.

### Required init sequence (48 MHz)
Because the part boots at **HSI 8 MHz**, `init.c` must:
1. Set **HSI = 24 MHz** (HSI trim/range select).
2. Set **`FLASH_ACR.LATENCY = 1`** *before* going above 24 MHz.
3. Select **`PLLSRC = HSI`**, enable PLL, wait for lock.
4. Switch SYSCLK to PLL (→ 48 MHz).

Structurally this is closest to the **G0/F0** path, so the existing
`clk_setup_pll()` shape fits — unlike C0, which needed the PLL removed entirely.

---

## 2. DMA — structurally identical to the STM32C031

| | |
|---|---|
| Channels | **3** (`DMA_CCR1/2/3`) — same as C031 |
| `DMA_ISR` / `DMA_IFCR` layout | **identical to STM32**: 4 flags per channel |
| IRQ vectors | **`DMA_Channel1`**, **`DMA_Channel2_3`** — same split as C031 |
| Request routing | **`SYSCFG_CFGR3`** — *not* a DMAMUX peripheral |
| Flexibility | any request → any channel (fully flexible) |

### ISR/IFCR bit layout
```
bits  0–3 : GIF1 TCIF1 HTIF1 TEIF1     (channel 1)
bits  4–7 : GIF2 TCIF2 HTIF2 TEIF2     (channel 2)
bits  8–11: GIF3 TCIF3 HTIF3 TEIF3     (channel 3)
bits 12–31: reserved
```
Four bits per channel → the flag for channel *N* is `flag << ((N-1)*4)`.
**This is the F0/C0 form**, i.e. the `(ch-1)*4` branch of `dma_clear_flag()` /
`dma_has_flag()` in `jdstm.h`. The trap to avoid: on STM32, `LL_DMA_CHANNEL_x` is 0-based on G0/L but 1-based on
F0/C0, and using the wrong shift clears the wrong channel's flags, so the
interrupt never clears (an interrupt storm, as first seen on the C0 port).
Puya's LL is 1-based (`LL_DMA_CHANNEL_1 = 0x1`), matching the `(ch-1)*4` shift.

### ⚠️ Request routing is the real porting difference
G0/C0 use `LL_DMA_SetPeriphRequest(DMA1, ch, LL_DMAMUX_REQ_x)`. **PY32 has no
DMAMUX peripheral** — routing is three 6-bit fields in **`SYSCFG_CFGR3`**
(offset `0x1C`):

| Bits | Field | |
|---|---|---|
| 20:16 | `DMA3_MAP` | request for channel 3 |
| 21 | `DMA3_ACKLVL` | response speed (0 normal / 1 quick) |
| 12:8 | `DMA2_MAP` | request for channel 2 |
| 13 | `DMA2_ACKLVL` | |
| 4:0 | `DMA1_MAP` | request for channel 1 |
| 5 | `DMA1_ACKLVL` | |

Request IDs:

| ID | Request | | ID | Request |
|---|---|---|---|---|
| `00000` | ADC | | `01000` | USART2_RX |
| `00001` | **SPI1_TX** | | `01001` | I2C_TX |
| `00010` | SPI1_RX | | `01010` | I2C_RX |
| `00011` | SPI2_TX | | `01011`–`10001` | TIM1 CH1-4/COM/UP/TRIG |
| `00100` | SPI2_RX | | `10010`–`10110` | TIM3 CH1/CH3/CH4/TRG/UP |
| `00101` | **USART1_TX** | | `11000`–`11001` | TIM16 CH1/UP |
| `00110` | **USART1_RX** | | `11010`–`11011` | TIM17 CH1/UP |
| `00111` | USART2_TX | | | |

`DMAn_ACKLVL` has **no STM32 equivalent** — worth experimenting with for WS2812
timing margin.

### Proposed channel allocation (mirrors our C0 port)
| Channel | Use | `SYSCFG_CFGR3` | IRQ |
|---|---|---|---|
| 1 | SPI1_TX — WS2812 output | `DMA1_MAP = 00001` | `DMA_Channel1` |
| 2 | USART1_TX — Jacdac bus | `DMA2_MAP = 00101` | `DMA_Channel2_3` |
| 3 | USART1_RX — Jacdac bus | `DMA3_MAP = 00110` | `DMA_Channel2_3` |

Since routing is fully flexible there's no channel-conflict puzzle — and this is
**the same allocation and the same shared-IRQ structure we already implemented
for the C031**, so the `uart.c` C0 branch should transfer almost verbatim.

---

## 3. Porting impact summary

| Area | Verdict |
|---|---|
| `init.c` clock | New third pattern; G0-like PLL path, but set HSI=24 first and `LATENCY=1`. Moderate, well-understood. |
| `jdstm.h` DMA flags | Use the **`(ch-1)*4`** form (F0/C0 branch). ✅ low risk |
| DMA request routing | **New code** — `SYSCFG_CFGR3` write replaces `LL_DMA_SetPeriphRequest()`. Small but must not be missed. |
| Channel allocation & IRQs | **Identical to C031** — ch1 SPI, ch2/3 USART, `Channel2_3` shared IRQ. ✅ transfers |
| EXTI vectors | `EXTI0_1` / `EXTI2_3` / `EXTI4_15` — same as STM32. ✅ |

---

## 4. USART baud rate (BRR) — for the 1 Mbaud Jacdac bus

> ⚠️ **Update after the port compiled (2026-09-23).** The BRR arithmetic below
> holds, but **the USART is the STM32F1 design**, not ST's modern USART:
> `SR/DR/BRR/CR1-3/GTPR`, no `ISR`/`ICR`/`RDR`/`TDR`/`BUSY`/`REACK`/`TEACK`. Two
> consequences matter most:
>
> - **`OVER8` is `CR3` bit 11.** In `CR1`, bit 11 is `WAKE`. Code that ORs
>   `LL_USART_OVERSAMPLING_8` into `CR1` gets 16× oversampling with an 8×
>   divisor, i.e. **half baud rate**. The port uses 16× with `BRR = 48`
>   throughout.
> - **Clearing `UE` does not reset the status flags**, unlike ST's USART. Errors
>   clear by reading `SR` then `DR` (Puya's `LL_USART_ClearFlag_FE/NE/ORE` do
>   exactly that).
>
> The `HSI_MHZ` naming trap (below) was resolved by running a *fixed* 48 MHz and
> defining `HSI_MHZ` as that clock. See `py32/py32f0.h` and
> [`py32-porting.md`](py32-porting.md).

### The formula is effectively the same as modern STM32
The RM presents it in the older mantissa/fraction style:

```
baud = fCK / (16 × USARTDIV)      USARTDIV = DIV_Mantissa + DIV_Fraction/16
```

but since `BRR` stores `USARTDIV × 16` (4 fractional bits), this reduces to the
familiar form:

```
BRR = fCK / baud          (oversampling by 16)
```

*Check against the RM's own example:* `BRR = 0x1BC = 444`, `USARTDIV = 27.75`,
and `444 = 27.75 × 16` ✓.

Oversampling **by 16 or by 8** is supported (`OVER8`; 0 = by 16), same as ST.

### For Jacdac at 1 Mbaud
| | |
|---|---|
| fCK | **PCLK = 48 MHz** (with APB prescaler `/1`) |
| Oversampling | 16 |
| **BRR** | **48 (0x30)** → `48 MHz / 48 = 1.000 Mbaud` |
| Error | **0.00 %** — exact |

This is the **same numeric value** the current code already writes
(`USARTx->BRR = HSI_MHZ;` where `HSI_MHZ` = 48 on C0), so the line carries over
unchanged — *subject to the trap below.*

### ⚠️ There is no USART clock mux — USART is hard-wired to PCLK
`RCC_CCIPR` on PY32 contains **only** `LPTIM1SEL`, `COMP1SEL`, `COMP2SEL`,
`PVDSEL` — **no `USART1SEL`**. Puya's LL has no `LL_RCC_SetUSARTClockSource()`
either.

Consequences for the port:
1. **Delete the `LL_RCC_SetUSARTClockSource(...)` call** in `uart.c` for PY32 —
   the API doesn't exist (on C0 we had to set it to `SYSCLK`).
2. **The APB1 prescaler must be `/1`** so PCLK = 48 MHz
   (`LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1)`), otherwise the baud rate is
   silently wrong.

### 🚨 Naming trap: `HSI_MHZ` no longer equals the USART clock
In `jacdac-stm32x0`, `HSI_MHZ` does double duty — the HSI frequency *and* the
USART `fCK` — because on F0/G0/C0 those coincide. **On PY32 they do not:**

| | HSI | SYSCLK / PCLK (USART fCK) |
|---|---|---|
| STM32C0 | 48 MHz | 48 MHz — same ✓ |
| **PY32F030** | **24 MHz** | **48 MHz** (HSI 24 × PLL 2) |

Defining `HSI_MHZ 24` for PY32 and leaving `BRR = HSI_MHZ` would yield
`BRR = 24` → **2 Mbaud**, and the Jacdac bus would simply never enumerate.

**Fix:** introduce an explicit `USART_FCK_MHZ` (= 48) for the BRR calculation
and stop overloading `HSI_MHZ`. Same applies to `target_wait_us()`, which scales
off `cpu_mhz` (48) — keep those two concepts separate.
