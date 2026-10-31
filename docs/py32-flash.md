# PY32F030 flash IP — the bootloader's foundation

From the **PY32F030 Reference Manual V1.7** §4. Background for the
port; see [`py32-porting.md`](py32-porting.md) for the summary.

> ⚠️ **This is the largest divergence from STM32 found so far.** Neither the F0
> (16-bit half-word) nor the G0/C0 (64-bit doubleword) programming path applies.
> `flash.c` needs a genuine **new branch**, not a rename.

---

## Geometry

| | STM32G030 | STM32C031 | **PY32F030** |
|---|---|---|---|
| Page (erase unit) | 2048 B | 2048 B | **128 B** |
| Sector | — | — | 4 KB |
| Main flash | 32 KB | 32 KB | **64 KB** |
| Info block | — | — | 4 KB (UID, option bytes, factory config) |

The **128-byte page is 16× smaller** than the G0/C0 page we build against today.

## Programming model — page-oriented, 32-bit only

Programming writes **a whole 128-byte page (32 × 32-bit words)** at a time:

1. Check `BSY` in `FLASH_SR`.
2. If the page already holds data, **read out its 32 words first** (read-modify-write).
3. Unlock: write **KEY1 `0x45670123`**, **KEY2 `0xCDEF89AB`** to `FLASH_KEYR`
   — *identical values to ST.* ✅
4. Set `PG` + `EOPIE` in `FLASH_CR`.
5. Write words **1 … 31** to the target address.
6. Set **`PGSTRT`** in `FLASH_CR`.
7. Write the **32nd word** → this *triggers* the program; hardware sets `BSY`.
8. Wait for `BSY` to clear.
9. Check `EOP` in `FLASH_SR`, clear it.
10. Clear `PG`.

> 🚨 **Only 32-bit accesses are permitted.** "Any non 32-bit write will cause a
> hard fault interrupt." The existing F0 path does **16-bit** writes — porting
> that verbatim would HardFault immediately.

## Page erase

1. Check `BSY`.
2. Unlock with KEY1/KEY2.
3. Set `PER` + `EOPIE` in `FLASH_CR`.
4. **Write arbitrary 32-bit data to any address in the target page** — the write
   *selects* the page. There is **no `FLASH_AR`** (F0) and **no `FLASH_CR.PNB`**
   (G0/C0) page-number field.
5. Wait for `BSY` to clear; check and clear `EOP`.

Mass erase (main flash only) and sector erase also exist; both are blocked when
WRP is enabled (`WEPERR` is set).

## 🚨 Mandatory: flash timing configuration

> "The write and erase time of the Flash **must be strictly controlled.
> Otherwise, the operation may fail.** … configure the Flash write and erase time
> control registers correctly [according to the HSI frequency]."

Registers: `FLASH_TS0`, `FLASH_TS1`, `FLASH_TS2P`, `FLASH_TPS3`, `FLASH_TS3`,
`FLASH_PERTPE`, `FLASH_SMERTPE`, `FLASH_PRGTPE`.

**Factory-calibrated values live in the info block** (`0x1FFF_0F00`, 128 B,
"factory config … flash erase time") and must be loaded into these registers
before any erase/program. **There is no STM32 equivalent** — miss this and flash
ops fail or corrupt intermittently, which would be miserable to debug.

**Mitigation: use Puya's calibration macros.** *(Corrected 2026-09-23 against
the official SDK.)* An earlier version of this note cited `LL_FLASH_Erase()`,
`LL_FLASH_GET_FLAG()` and `LL_FLASH_IS_INVALID_TIMMING_SEQUENCE()`. **Those
come from the community mirror `IOsetting/py32f0-template`, whose
`py32f0xx_ll_flash.c` is not in Puya's official v1.5.0 release.** The official
`py32f0xx_ll_flash.h` is header-only and provides:

- **one calibration macro per HSI setting**,
  `LL_FLASH_TIMMING_SEQUENCE_CONFIG_{4M,8M,16M,22P12M,24M}()`, each loading the
  eight timing registers from the factory info block. We run HSI at 24 MHz, so
  we use **`_24M()`**;
- inline `LL_FLASH_Unlock()`, `LL_FLASH_Lock()`, `LL_FLASH_PageProgram()`, and
  the erase-mode enables.

Puya's HAL re-applies the calibration before every erase and program
(`__HAL_FLASH_TIMMING_SEQUENCE_CONFIG()`, keyed off `RCC_ICSCR.HSI_FS`).
`flash.c` should do the same with the `_24M()` macro. It's cheap, and it
survives any later change to the HSI setting.

---

## Porting impact

| Item | Impact |
|---|---|
| `flash.c` `flash_program()` | **Rewrite.** Page-buffered 31-words + `PGSTRT` + 32nd-word trigger. Neither existing branch fits. |
| `flash.c` `flash_erase()` | **Rewrite.** `PER` + dummy 32-bit write to the page; no `AR`/`PNB`. |
| Access width | Must be **32-bit everywhere** — 8/16-bit writes HardFault. |
| Unlock keys | ✅ unchanged from ST |
| `JD_FLASH_PAGE_SIZE` | **128** (was 2048). Set in the MCU `.mk`. |
| Flash timing setup | **New required step:** `LL_FLASH_TIMMING_SEQUENCE_CONFIG_24M()` before each erase/program. |
| `BL_SIZE` / layout | Re-derive; 64 KB total gives generous bootloader room. |

### ❓ To verify next
The **128-byte page** is much smaller than anything the current build targets
(F0 1 KB, G0/C0 2 KB). Confirm that `jacdac-c`'s firmware-update path and
`scripts/patch-bin.js` (which takes `PAGE_SIZE`) behave sensibly at 128 B — this
is the most likely place for a hidden assumption to bite.

---

# 128-byte pages vs the Jacdac bootloader protocol

Traced through `jacdac-stm32x0/bl/` and `jacdac-c` in the firmware repo, since
every existing target uses 1–2 KB pages and this was the likeliest place for a
hidden assumption.

## ✅ Verdict: the device side works at 128 B

**Page size is negotiated, not assumed.** The bootloader announces it in its
info report (`jd_bootloader_info_report_t { service_class, page_size,
flashable_size, product_identifier }`), built from `BL_PAGE_SIZE`:

```c
// bl/blproto.c
static const uint32_t bl_ad_data[] = { JD_SERVICE_CLASS_BOOTLOADER, BL_PAGE_SIZE,
                                       JD_FLASH_SIZE - BL_SIZE, 0 };
```

The transfer path is safe and correct at 128 B:
- `bl/blproto.h` defines `BL_SUBPAGE_SIZE 208` (max payload per packet) while
  `BL_PAGE_SIZE = JD_FLASH_PAGE_SIZE`. With a 128-byte page the page is *smaller
  than one subpage* — but that's handled, not broken.
- **Writes are bounds-checked**, so an oversized chunk is *rejected*, not
  overflowed into `pagedata[]`:
  ```c
  if (d->page_offset >= BL_PAGE_SIZE || d->page_offset + datasize > BL_PAGE_SIZE)
      ctx->subpageerr = 3;   // JD_BOOTLOADER_ERROR_INVALID_PAGE_OFFSET
  ```
- A 128-byte page arrives as a **single chunk** (`chunk_no = 0`,
  `chunk_max = 0`) → `isFinal` is true → `bl_write_page()` runs. Correct.
- `BL_SIZE` 4 KB ÷ 128 B = 32 pages — cleanly page-aligned. ✅
- `scripts/patch-bin.js` merely *records* `page_size` in the config blob; it does
  no page-based padding or chunking. ✅

## 🎁 Two incidental wins

1. **~1.9 KB of bootloader RAM saved.** `ctx_t` contains
   `uint8_t pagedata[JD_FLASH_PAGE_SIZE]` — that's **128 bytes instead of 2048**.
   Welcome on an 8 KB part.
2. **A 1:1 mapping with the silicon.** PY32 programs a whole 128-byte page in one
   sequence, and one bootloader page write is exactly one flash page — so
   `flash_program()` becomes a single page-program operation. Cleaner than G0,
   where a 2048-byte page needs 256 doubleword writes.

## ⚠️ Two things to watch

1. **Host-side flasher behaviour.** ✅ **Resolved 2026-09-23** by reading the
   host, jacdac-ts `src/jdom/flashing.ts`:
   - **Chunking is fine.** `flashPage()` clamps each chunk
     (`if (suboff + sz > pageSize) sz = pageSize - suboff`), so a 128-byte page
     goes as one 128-byte chunk with `chunk_no = chunk_max = 0`. The device
     treats that as first and last chunk, and writes the page.
   - 🚨 **The UF2 parser was the real problem.** `parseUF2()` takes the page
     size from the UF2's `0x0be9f7` tag (128 here) but copies **each UF2 block
     into a single page**. We generated 256-byte blocks, so the copy threw
     `RangeError: offset is out of bounds` and no bus update could start.
     **Fixed** in `scripts/bin2uf2.js` (payload = `min(256, page size)`), and
     checked on every build by `test/uf2-host-check.js`, a port of the parser.
   - The boot-block protection (page 0 sent first with the app reset vector
     blanked, then again last with the real one) survives: the host's parser
     keeps both copies, in order.
2. **Field updates will involve many more round trips.** For a ~15 KB app:

   | | 2048-byte page (G0/C0) | **128-byte page (PY32)** |
   |---|---|---|
   | Pages | ~8 | **~119** |
   | Chunks | ~80 | ~119 (1.5×) |
   | Page acks + erases | 8 | **119 (≈15×)** |

   Each page costs an erase + program + ack round-trip. **Estimated from the
   host code and datasheet (2026-09-23):** the host pauses 5 ms after each chunk
   (`BL_PAGE_DELAY`) and polls for the ack every 5 ms. PY32 programs a page in
   1.0 ms (1.5 max) and erases it in 3.0 ms (4.5 max). That's ~15 ms per page:
   **~1.3 s for an 88-page button app, ~1.7 s for the 111-page neopixel**, vs
   roughly 0.5-0.8 s on a G030. About 2×, not 15×. Confirm on hardware.
   Endurance is 100 k erase/write cycles (−40 to 85 °C) with 20-year retention.
   The datasheet doesn't state a limit on programs per page between erases,
   which the settings store's partial-page writes rely on.

## 🐛 Latent upstream bug spotted

```c
// bl/blproto.h
#define BL_NUM_SUBPAGES ((BL_PAGE_SIZE + SUBPAGE_SIZE - 1) / SUBPAGE_SIZE)
```
references **`SUBPAGE_SIZE`**, which is undefined — it should be
`BL_SUBPAGE_SIZE`. Harmless today because the macro is unused, but it would fail
to compile the moment anyone used it. Worth reporting upstream alongside the C0
work.
