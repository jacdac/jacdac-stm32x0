# Puya PY32F0 port: notes for maintainers

The `py32f0` family adds Puya's **PY32F030** and **PY32F003** (Cortex-M0+) to
`jacdac-stm32x0`. Puya's low-level (LL) driver library is a fork of ST's, so the
port reuses the existing `stm32/` drivers with PY32 branches, like the F0, G0 and
C0 families. This page records what differs from ST, the bugs found on the way,
and how to check a board.

Reference notes for each area:

| Topic | File |
|---|---|
| Pinouts, alternate functions, the TSSOP-20 and SOP-16 variants | [`py32-pinouts.md`](py32-pinouts.md) |
| Clock tree, DMA, USART baud rate | [`py32-clock-dma.md`](py32-clock-dma.md) |
| Flash controller, 128 B pages, UF2 | [`py32-flash.md`](py32-flash.md) |
| Puya's SDK and LL layer | [`py32-sdk.md`](py32-sdk.md) |
| Where the vendored SDK files came from | [`../py32/VENDORED-PUYA.md`](../py32/VENDORED-PUYA.md) |
| Flashing over SWD with pyOCD | [`../tools/README.md`](../tools/README.md) |

## Supported parts

| MCU file | Part | Flash / RAM | Clock |
|---|---|---|---|
| `PY32F030x8` | PY32F030F28P6TU (TSSOP-20, "F2" pinout), PY32F030K28U6 (QFN-32) | 64 KB / 8 KB | 48 MHz (HSI 24 MHz, PLL x2) |
| `PY32F003x6` | PY32F003W16S6TU (SOP-16, "W1" pinout) | 32 KB / 4 KB | 24 MHz (no PLL) |
| `PY32F003x8` | PY32F003 W18 | 64 KB / 8 KB | 24 MHz |

The PY32-only files are in `py32/`: the family header (`py32f0.h`, which also
holds the compatibility shim), the build files (`py32/mk/`) and the vendored SDK.
A target selects the family with:

```make
MCU = PY32F030x8
include $(JD_STM)/py32/mk/py32f0.mk
```

`stm32/mk/stm32.mk` gained one hook for this, `MCU_MK`, so a family kept outside
`stm32/` can point at its own per-MCU memory map. ST builds are unaffected.

## Port map

| File | PY32 change |
|---|---|
| `py32/py32f0.h` | Family header: LL and CMSIS includes, clock constants, shims (USART flag helpers, DMA request routing, `NVIC_GetEnableIRQ`) |
| `stm32/init.c` | Clock setup per part; `entry()` (Puya's startup calls `entry`, not `main`) |
| `stm32/uart.c`, `bl/bluart.c` | F1-style USART: flag handling, half duplex, TC handling |
| `stm32/jdstm.h` | Family select; DMA flags use the 1-based `(ch-1)*4` form, as F0 and C0 |
| `stm32/flash.c` | Rewritten for 128 B page programming |
| `stm32/adc.c` | No `ADRDY`/`ADDIS`, ADEN auto-clear, clock limit, factory temperature calibration |
| `stm32/exti.c` | F0-style single `PR`; port selector on lines 0 to 8 only |
| `stm32/rtc.c` | F1-style counter RTC: Sleep mode only for now |
| `stm32/pwm.c`, `bl/blpwm.c` | PY32 timer and alternate-function tables |
| `stm32/dspi.c`, `stm32/spidef.h` | WS2812 over SPI1 and DMA channel 1 |
| `stm32/target_utils.c`, `bl/blutils.c` | Busy-wait timing |
| `src/jd_user_config.h` | Larger `JD_STACK_SIZE` on PY32 (see "Stack") |
| `scripts/bin2uf2.js` | UF2 payload sized for 128 B pages |

## PY32 is less ST-like than its API

Puya's LL keeps ST's function names, but several peripherals are an older design.
That only shows once code touches registers.

| Peripheral | What PY32 has |
|---|---|
| **USART** | The **STM32F1 register set**: `SR/DR/BRR/CR1-3/GTPR`. No `ISR`, `ICR`, `RDR`/`TDR`, `BUSY`, `REACK`/`TEACK`. Errors clear by reading `SR` then `DR`, and clearing `UE` does not reset them. `OVER8` is `CR3` bit 11 (in `CR1` that bit is `WAKE`). No clock mux: the USART runs from PCLK |
| **RTC** | STM32F1-style counter RTC (`CNT/ALR`), not ST's calendar RTC |
| **ADC** | F0-style, but no `ADRDY` and no `ADDIS`, and **ADEN clears after every conversion** (bug 8). Temperature sensor on channel 11, VREFINT on 12. Factory temperature calibration at `0x1FFF0F14` / `0x1FFF0F18`. Maximum ADC clock 12 MHz |
| **EXTI** | F0-style single `PR`. The port selector covers lines 0 to 8 only; lines 9 to 15 are wired to port A. Different line encoding from G0 |
| **DMA** | 3 channels, 1-based, `Channel1` and `Channel2_3` IRQs, as the C031. No DMAMUX: request routing is a 6-bit field per channel in `SYSCFG_CFGR3` |
| **Flash** | 128 B pages programmed a page at a time, 32-bit writes only, erase by a dummy write, timing registers loaded from factory values per HSI setting |
| **RCC, SPI, GPIO** | Close to ST: G0-style `APBENR1/2`, F0-style SPI and GPIO |
| **CMSIS core** | V4.30: no `NVIC_GetEnableIRQ()` (shimmed) |

**Clock.** The F030 runs a fixed 48 MHz from HSI 24 MHz and the fixed x2 PLL. The
F003 has no PLL and runs a fixed 24 MHz. `HSI_MHZ` in `py32f0.h` means "the fixed
system, PCLK and USART clock" (48 or 24), which keeps the shared code unchanged:
`BRR = HSI_MHZ` gives exactly 1 Mbaud on both. The oscillator frequency itself is
`PY32_HSI_OSC_MHZ`.

## Bugs found, and how

### Before hardware (code reading and datasheets)

Each of these would have compiled and then failed on silicon.

1. **Bootloader at half baud rate.** Writing `LL_USART_OVERSAMPLING_8` into `CR1`
   sets `WAKE`, leaving 16x oversampling with an 8x divisor. Both UARTs now use
   16x with `BRR = 48`.
2. **UART error interrupt storm.** On ST, clearing `UE` resets the error flags,
   and the error path relies on it. PY32 keeps them, so a leftover `FE`/`ORE`
   would re-assert the interrupt forever. `uart_disable()` now clears flags and
   DMA requests explicitly.
3. **No WS2812 output.** `px_init()` routes the DMA request separately from
   `spidef.h` and needed its own PY32 branch.
4. **EXTI on the wrong pin.** The G0 formula for `LL_EXTI_SetEXTISource` gives
   the wrong encoding on PY32.
5. **Flash panic on every write**, had ST's "`EOP` must be set" check been kept.
   Puya's HAL never checks `EOP`; PY32 verifies by read-back.
6. **No firmware update over the bus.** `scripts/bin2uf2.js` wrote 256 B UF2
   blocks. The host flasher (jacdac-ts, `src/jdom/flashing.ts`) copies each block
   into one flash page, so with 128 B pages its parser threw a `RangeError`.
   Fixed: the payload is `min(256, page size)`. ST output is byte-identical.
   `test/uf2-host-check.js` ports the host's parser and runs on every build.
7. **ADC clocked at twice its limit.** PCLK/2 is 24 MHz on the F030; the maximum
   is 12 MHz. PY32 uses PCLK/4. Also, Puya's `LL_ADC_DELAY_TEMPSENSOR_STAB_US` is
   10 us against a datasheet start-up of up to 120 us; PY32 uses 120 us.

A review before hardware also found and fixed:

- TX could truncate packets: enabling TE sends an idle frame that sets `TC` again,
  so `TC` is cleared again just before the TX DMA starts.
- The bootloader's "line busy" check read only the pin, so high bits mid-frame
  looked idle. Busy is now "pin low, or `RXNE` set with no `IDLE` since".
- The bootloader's receive loop swallowed the next frame's break (reading `DR`
  clears `FE` on F1-style parts). `FE` is now tested before `RXNE`.
- `SPI_RX` on PY32 would take DMA channels 2 and 3 from the Jacdac UART. It is
  now an `#error` in `spidef.h`.
- Puya's `LL_ADC_Disable()` re-enables interrupts unconditionally. PY32 uses a
  copy that restores PRIMASK.

### On silicon (PY32F030K28U6)

8. **ADEN clears after every conversion.** After the first flash, the bootloader
   hung in `bl_adc_random_seed()` waiting for `EOC`, with `ADC_CR` reading 0.
   Writing `ADC_CR` by hand over SWD showed that each software-triggered
   conversion switches the ADC off, and a start request with the ADC off is
   ignored. Puya's LL doesn't mention it. Fix: `py32_adc_ensure_enabled()`
   before every conversion, in `adc_convert()` and the bootloader's seed loop.
9. **Busy-waits ran 25% short.** The `subs`/`bne` loop assumes 4 cycles per
   iteration; Cortex-M0+ takes the branch in 2, so G0 and C0 pad it with a
   `nop`. PY32 wasn't in that list. The frame-start pulse came out at about
   8.3 us instead of 11, and the gap before data at about 34 us, below Jacdac's
   50 us minimum. Fix: PY32 added to the `nop` list in `stm32/target_utils.c`
   and `bl/blutils.c`.
10. **Every transmitted frame lost its last byte.** On the F1-style USART, `TC`
    sets at any gap between bytes, and DMA latency leaves small gaps. A DMA write
    to `DR` doesn't clear it, unlike ST's newer USART, so `DMA_Handler`'s wait for
    `TC` returned at once (0.4 us) and the end-of-frame pulse cut off the final
    byte. Fix: clear `TC` on entry to the handler, before waiting. The wait is now
    8.5 us and frames pass CRC.

**Debug aid: `-DPY32_TX_CAPTURE`** (off in normal builds; build with
`USERFLAGS=-DPY32_TX_CAPTURE`). It samples the bus pin from a RAM loop while the
third frame goes out, and again while `DMA_Handler` waits for `TC`, with SysTick
marks at the start pulse, DMA start, `TC` and end pulse, plus a copy of the bytes
meant to go out. Everything can be read and decoded over SWD without a logic
analyser. Bugs 9 and 10 were found with it.

## RAM

Measured on PY32 builds:

| Image | App flash | App `.bss` | Bootloader flash | Bootloader `.bss` |
|---|---|---|---|---|
| Button, 4 KB RAM recipe (F003 SOP-16) | 10,976 | 748 | 3,416 | 696 |
| Button, 4 KB RAM recipe (F030) | 11,060 | 748 | 3,488 | 696 |
| Slider, 4 KB RAM recipe (F030 dev board) | 11,156 | 748 | 3,492 | 696 |
| WS2812 strip, full build (F030) | 13,988 | 2,868 | 3,488 | 696 |

**The 4 KB RAM recipe** is two flags:

```make
DEFINES += -DDEVICE_DMESG_BUFFER_SIZE=0 -DJD_CONFIG_CONTROL_FLOOD=0
```

`DEVICE_DMESG_BUFFER_SIZE=0` removes both 1 KB debug log buffers (the platform's
`codalLogStore` and jacdac-c's `jacdacLogStore`, whose size defaults to it). The
bootloader already builds this way. `JD_CONFIG_CONTROL_FLOOD=0` drops the
flood-ping bus test command (about 200 B of flash). Other knobs cost more than
they save: `JD_CONFIG_STATUS=0` does not link, and `JD_CONFIG_WATCHDOG=0` drops
the control service's `reset_in` register.

A button then needs about **2.1 KB of RAM in the worst case**: 748 B static,
700 B of heap (two 252 B transmit frames, the 128 B event queue, service state)
and a stack bounded at 664 B. Four full-size 252 B frame buffers are about half
of that, so going much lower than 2 KB needs changes to frame buffering (one
receive or transmit buffer, or a smaller maximum frame), not configuration.

**The bootloader** fits easily: its `pagedata[]` buffer is one flash page, so
PY32's 128 B page cuts bootloader `.bss` from about 2.6 KB on ST to 696 B. On any
small part, check the page size before the RAM size.

## Stack

`jacdac-c`'s allocator keeps `JD_STACK_SIZE` (512 B by default) free above the
heap. `test/stack-usage.py` bounds the real need from GCC's own data:

```bash
USERFLAGS="-fstack-usage -fcallgraph-info=su" test/build-check.sh <target>
python3 test/stack-usage.py built/<target>/app      # or built/<target>/bl
```

It follows GCC's call graph, charges calls through function pointers to their
real targets (timer and EXTI callbacks, service vtables, DMA handlers), nests
interrupts by priority with 36 B of exception frame per level, and charges
library calls 32 B each. It prints one bound for all paths and one without the
panic handlers.

| Image | All paths | Without panic |
|---|---|---|
| Button, 4 KB recipe | 664 B | 624 B |
| Slider, 4 KB recipe | 600 B | 560 B |
| WS2812 strip, DMESG on | 1,620 B | 1,492 B |
| WS2812 strip, DMESG off | 740 B | 676 B |
| Bootloader | 304 B | 304 B |

**512 B is too small on every app image.** It matters most for the LED strip,
whose `alloc()` takes all the heap left and so leaves the stack exactly the
reserve. On PY32, `src/jd_user_config.h` sets `JD_STACK_SIZE` to 1,024 B with
DMESG off and 1,792 B with it on; a `board.h` can override it, and ST builds keep
512 B.

The bound is an upper bound (every handler at a level is assumed to preempt at
its deepest point). Two functions have small dynamic frames that aren't counted
(`jd_services_announce`, `sensor_handle_packet`). A new kind of callback shows up
as a warning, never as a silently low figure.

## Firmware update over the bus

Page size is announced by the bootloader, not assumed, and the device side
handles 128 B pages: a page arrives as one chunk, chunk writes are bounds-checked,
and Puya's page program maps 1:1 to a bootloader page write. With the UF2 fix
above, a button should update in about 1.3 s (an estimate: 88 pages at about
15 ms each), against 0.5 to 0.8 s on an STM32G030. An update from the Jacdac
dashboard has been verified on silicon.

## Known gaps

| Gap | Effect | To close |
|---|---|---|
| No deep sleep | `rtc_sleep()` always uses Sleep mode (WFI). Correct, but higher idle current than STOP | A driver for the F1-style counter RTC |
| `RESET_AS_GPIO` not implemented | PF2 stays NRST (the factory default); `RESET_AS_GPIO` is a compile error | Puya's option-byte sequence (1-bit `NRST_MODE`) |
| `TS_CAL2` temperature unknown (85 or 105 °C; Puya's header says either) | If 105 °C, temperature over-reads by about 36% of (T minus 30 °C); small near room temperature | A warm reading against a thermometer, then pick `__LL_ADC_CALC_TEMPERATURE_85` or `_105` |
| No PY32F002A | It has no DMA, so the DMA-driven UART can't run | An interrupt-driven UART |
| `SPI_RX` unavailable | It would take the UART's DMA channels | `#error` in `spidef.h` |

## Also affects ST (candidates for separate fixes)

Found while porting; not changed here for ST:

| What | Where | Note |
|---|---|---|
| Bootloader overwrite guard never fires | `bl/blproto.c` `page_data()` | Compares an offset (`page_address - 0x8000000`) with an absolute address (`&bl_dev_info - BL_PAGE_SIZE`), so it is always false. Harmless in practice (hosts only send app pages) |
| `BL_NUM_SUBPAGES` uses undefined `SUBPAGE_SIZE` | `bl/blproto.h` | Unused, but won't compile if used |
| `clk_is_pll()` compares the SWS status field with the SW selector | `stm32/init.c` | Always false; no callers. Fixed for PY32 only |
| `SPI_RX` on C0 collides with the UART's DMA channels 2 and 3 | `stm32/spidef.h` | An `#error` on PY32; silent on C0 |
| `JD_STACK_SIZE` default of 512 B | jacdac-c `source/interfaces/simple_alloc.c` | Below the static worst case of a stock Cortex-M0+ build (see "Stack") |
| LED strip pixel buffer allocated in words, not bytes | jacdac-c `services/ledstrip.c` `alloc()` | `jd_alloc(PX_WORDS(n))` passes a word count where bytes are expected. Harmless only while it is the last allocation. Fix: `jd_alloc(PX_WORDS(n) * 4)` |

## Building and test targets

`test/build-check.sh` compiles PY32 targets with no hardware. It assembles a
throwaway top-level module repo from this repo, an upstream `jacdac-c` checkout
(with its `jacdac` spec submodule) and the targets in `test/targets/`:

```bash
test/build-check.sh /path/to/jacdac-c                  # every target
test/build-check.sh /path/to/jacdac-c py32f003w16-button
```

Outputs (`combined-*.hex` for a first SWD flash, `app-*.uf2` for bus updates) go
to `built/<target>/`, and every UF2 is checked with the host parser.

| Target | What it is |
|---|---|
| `py32f030-button-test` | Compile test: button on PY32F030F28P6TU |
| `py32f003w16-button` | Button on PY32F003W16S6TU (SOP-16, bus on PA2 @ AF1 since PB6 isn't bonded) |
| `py32f030k28-devboard-button` | Bring-up: Embedfire PY32F030K28U6 core board, its two keys as buttons |
| `py32f030k28-devboard-slider` | Bring-up: the same board as a potentiometer module |

## Dev-board bring-up

The bring-up board is the **Embedfire PY32F030K28U6 core board** (EBF410454
V1R0, QFN-32, same 64 KB / 8 KB as the TSSOP-20 part).

- **SWD** on J4 (RST, CLK, GND, DIO, 3V3). Flash with
  `tools/flash.sh py32f030k28-devboard-button --erase`.
- **Onboard parts:** LED4 on PA4 (active low; status and bootloader LED), LED2 and
  LED3 on PA2 and PA3, KEY1 and KEY2 on PA5 and PA6 (to GND, active low). Y1 is
  24 MHz on PF0/PF1 and Y2 (presumably 32.768 kHz) on PA9/PA10; the firmware uses
  neither.
- **Bus wiring:** Jacdac data through 220 Ω to **PB6** (left header J3, seventh
  from the top), and GND. Power the board from its own USB-C (power only) or the
  probe's 3V3, not both, and not from `JD_PWR`.
- **Slider target:** wiper to PA0, one end to PB7 (driven only while sampling),
  the other end to GND.

### Hardware checklist, in order

Start with the button target: it needs only clock and bus to enumerate.

1. **Clock.** 48 MHz: check `target_wait_us()` or system time against a known
   delay. *(Done: 1.011 s over a 1 s SWD pause.)*
2. **Bus.** The module enumerates and its buttons work in the dashboard. This
   proves the clock, the F1-style USART handling, DMA routing and EXTI on PB6.
   *(Done.)*
3. **Error path.** Corrupt frames (hold the bus low from SWD for 5 ms) and
   confirm no interrupt storm and no reset. *(Done: a `bus_timeout_error` is
   logged and reception continues.)*
4. **Status LED.** Each colour on its timer channel.
5. **Flash.** Settings write and erase, including a partial-page write.
6. **Bootloader and UF2 over the bus.** Interrupt one update partway and check
   the module stays in the bootloader. *(Update done; the interruption test is
   open.)*
7. **WS2812** output via SPI1 and DMA channel 1.
8. **ADC and temperature** against the factory calibration. Read `TS_CAL1` and
   `TS_CAL2` with
   `pyocd cmd --pack tools/Puya.PY32F0xx_DFP.1.2.6.pack -t py32f030x8 -c "read16 0x1FFF0F14 8"`.
   Then the slider: about 0 and 1 at the ends, smooth in between.
9. **NRST** behaves as reset.
10. **Stack high-water mark**, to check the static bound. Run on a busy bus for a
    few minutes (including an identify and an update request), halt, and dump
    the reserve: `jd_alloc_init()` paints it with `0x33333333`, and the used
    stack is everything above the last painted word. The reserve starts at
    `_estack` minus `JD_STACK_SIZE`.
