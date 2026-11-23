#ifndef __STM_H
#define __STM_H

#include "board.h"

// Puya PY32F0 family (PY32F030 / PY32F003 / PY32F002A): Cortex-M0+ parts whose
// LL drivers are a fork of ST's (BSD-3, see VENDORED-PUYA.md). This header plays
// the role stm32g0.h / stm32c0.h play for ST parts, plus a small compatibility
// shim so the shared platform code needs as few PY32 #if branches as possible.
#define PY32F0 1

#include "py32f0xx_ll_crc.h"
#include "py32f0xx_ll_bus.h"
#include "py32f0xx_ll_gpio.h"
#include "py32f0xx_ll_exti.h"
#include "py32f0xx_ll_dma.h"
#include "py32f0xx_ll_rcc.h"
#include "py32f0xx_ll_system.h"
#include "py32f0xx_ll_cortex.h"
#include "py32f0xx_ll_utils.h"
#include "py32f0xx_ll_pwr.h"
#include "py32f0xx_ll_usart.h"
#include "py32f0xx_ll_tim.h"
#include "py32f0xx_ll_spi.h"
#include "py32f0xx_ll_adc.h"
#include "py32f0xx_ll_rtc.h"
#include "py32f0xx_ll_i2c.h"
#include "py32f0xx_ll_flash.h"

// ---------------------------------------------------------------------------
// Clock
//
// PY32 runs a FIXED system clock, set up once in SystemInit() and never switched
// at runtime (the same model as STM32C0: DISABLE_PLL below makes clk_set_pll() a
// no-op, including around sleep in pwr.c). The speed depends on the part:
//
//  - PY32F030: 48 MHz = HSI at 24 MHz, doubled by the PLL (a fixed x2, with no
//    multiplier field). One flash wait state.
//  - PY32F003 / PY32F002A: 24 MHz = HSI at 24 MHz, directly. These parts have NO
//    PLL, and are rated to 32 MHz only with an external crystal (PY32F003
//    datasheet V2.7). Zero flash wait states.
//
// That matters more here than on ST parts: PY32 has NO USART clock mux. The
// USART is clocked from PCLK, so any runtime clock switch would change the
// Jacdac baud rate. On G0 the USART runs from HSI16 via its own mux, which is
// why the shared code can write BRR = HSI_MHZ there.
//
// So on PY32, HSI_MHZ means "the fixed system / PCLK / USART clock" (48 or 24),
// which is not always the HSI oscillator frequency (24). The shared code uses
// HSI_MHZ for cpu_mhz, the timers, the busy-waits and USART BRR: BRR = 48 at
// 48 MHz, or 24 at 24 MHz, is exactly 1.000 Mbaud either way. The oscillator
// frequency is PY32_HSI_OSC_MHZ.
#define PY32_HSI_OSC_MHZ 24
#if defined(PY32F030x3) || defined(PY32F030x4) || defined(PY32F030x6) ||                    \
    defined(PY32F030x7) || defined(PY32F030x8)
#define PY32_HAS_PLL 1
#define HSI_MHZ 48
#elif defined(PY32F003x4) || defined(PY32F003x6) || defined(PY32F003x8) || defined(PY32F002Ax5)
#define PY32_HAS_PLL 0
#define HSI_MHZ 24
#else
#error "unknown PY32 part: add it to the clock table in py32f0.h"
#endif
// the fixed system clock; named PLL_MHZ for parity with the ST families
#define PLL_MHZ HSI_MHZ
#define DISABLE_PLL 1

// ---------------------------------------------------------------------------
// ST HAL clock-enable macros used by the shared platform code, mapped onto
// Puya's LL bus calls (the Puya HAL is not vendored). Only peripherals that
// exist on PY32F030 are defined: there is no TIM2, DMA2, DMAMUX or UART4, so
// code needing those must stay behind its ST family #if.
#define __HAL_RCC_DMA1_CLK_ENABLE() LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1)
#define __HAL_RCC_FLASH_CLK_ENABLE() LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_FLASH)
#define __HAL_RCC_USART1_CLK_ENABLE() LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_USART1)
#define __HAL_RCC_USART2_CLK_ENABLE() LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2)
#define __HAL_RCC_TIM1_CLK_ENABLE() LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM1)
#define __HAL_RCC_TIM3_CLK_ENABLE() LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3)
#define __HAL_RCC_TIM14_CLK_ENABLE() LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM14)
#define __HAL_RCC_TIM16_CLK_ENABLE() LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM16)
#define __HAL_RCC_TIM17_CLK_ENABLE() LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM17)
#define __HAL_RCC_SPI1_CLK_ENABLE() LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SPI1)
#define __HAL_RCC_SPI1_FORCE_RESET() LL_APB1_GRP2_ForceReset(LL_APB1_GRP2_PERIPH_SPI1)
#define __HAL_RCC_SPI1_RELEASE_RESET() LL_APB1_GRP2_ReleaseReset(LL_APB1_GRP2_PERIPH_SPI1)
#define __HAL_RCC_ADC_CLK_ENABLE() LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_ADC1)
#define __HAL_RCC_ADC_CLK_DISABLE() LL_APB1_GRP2_DisableClock(LL_APB1_GRP2_PERIPH_ADC1)
#define __HAL_RCC_ADC1_CLK_ENABLE() __HAL_RCC_ADC_CLK_ENABLE()
#define __HAL_RCC_ADC1_CLK_DISABLE() __HAL_RCC_ADC_CLK_DISABLE()
#if defined(SPI2)
#define __HAL_RCC_SPI2_CLK_ENABLE() LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2)
#endif

// ---------------------------------------------------------------------------
// EXTI naming. Puya's LL drops ST's "_0_31" suffix, and its EXTI is F0-style:
// one pending register (EXTI->PR) with a single flag per line, rather than G0's
// separate rising/falling pending registers. Clearing "the falling flag"
// therefore clears the line's only flag.
//
// NOT aliased: LL_EXTI_SetEXTISource(). Puya's has the same name but a
// different line encoding, and only lines 0-8 have a port selector (lines 9-15
// are wired to port A). exti.c has a dedicated PY32 branch for it.
#define LL_EXTI_EnableIT_0_31 LL_EXTI_EnableIT
#define LL_EXTI_DisableIT_0_31 LL_EXTI_DisableIT
#define LL_EXTI_EnableFallingTrig_0_31 LL_EXTI_EnableFallingTrig
#define LL_EXTI_EnableRisingTrig_0_31 LL_EXTI_EnableRisingTrig
#define LL_EXTI_ClearFlag_0_31 LL_EXTI_ClearFlag
#define LL_EXTI_ClearFallingFlag_0_31 LL_EXTI_ClearFlag
#define LL_EXTI_ClearRisingFlag_0_31 LL_EXTI_ClearFlag
#define LL_EXTI_IsActiveFlag_0_31 LL_EXTI_IsActiveFlag
#define LL_EXTI_IsActiveFallingFlag_0_31 LL_EXTI_IsActiveFlag

// ---------------------------------------------------------------------------
// USART. PY32's USART has the STM32F1 register set (SR/DR/BRR/CR1-3/GTPR): no
// ISR, ICR, RDR/TDR, BUSY, REACK or TEACK, and OVER8 lives in CR3 bit 11. These
// helpers are shared by the app UART (stm32/uart.c) and the bootloader
// (bl/bluart.c), so the F1-style semantics live in one place.
//
// Clearing PE/FE/NE/ORE/IDLE (and RXNE) takes a read of SR followed by a read of
// DR; that's what Puya's LL_USART_ClearFlag_FE/NE/ORE do, so one call clears all
// of them. Unlike ST's USART, clearing UE does NOT reset these flags.
static inline void py32_usart_clear_errors(USART_TypeDef *u) {
    LL_USART_ClearFlag_ORE(u); // SR then DR read
}
// TC is rc_w0: writing 0 clears it. Enabling TE sends an idle frame, which SETS
// TC, so clear it again immediately before starting a DMA transmission (as the
// F1 manual prescribes), or a later "wait for TC" returns before the last byte.
static inline void py32_usart_clear_tc(USART_TypeDef *u) {
    LL_USART_ClearFlag_TC(u);
}

// ---------------------------------------------------------------------------
// SPI: Puya spells the DMA "last transfer" bits with an underscore.
#define SPI_CR2_LDMATX SPI_CR2_LDMA_TX
#define SPI_CR2_LDMARX SPI_CR2_LDMA_RX

// ---------------------------------------------------------------------------
// DMA request routing. PY32 has no DMAMUX: each of the 3 channels' request
// source is a field in SYSCFG_CFGR3, set with one LL function per channel. This
// stands in for ST's LL_DMA_SetPeriphRequest(). Needs the SYSCFG clock, which
// SystemInit() enables. ch is LL_DMA_CHANNEL_1..3 (1-based on PY32).
static inline void py32_dma_set_request(uint32_t ch, uint32_t req) {
    if (ch == LL_DMA_CHANNEL_1)
        LL_SYSCFG_SetDMARemap_CH1(req);
    else if (ch == LL_DMA_CHANNEL_2)
        LL_SYSCFG_SetDMARemap_CH2(req);
    else
        LL_SYSCFG_SetDMARemap_CH3(req);
}

// ---------------------------------------------------------------------------
// CMSIS: Puya ships CMSIS core V4.30, which predates NVIC_GetEnableIRQ() (added
// in CMSIS 5). Same implementation as CMSIS 5 for a Cortex-M0+ (one ISER word).
static inline uint32_t NVIC_GetEnableIRQ(IRQn_Type IRQn) {
    if ((int32_t)IRQn >= 0)
        return (NVIC->ISER[0U] & (1UL << (((uint32_t)IRQn) & 0x1FUL))) != 0UL ? 1UL : 0UL;
    return 0U;
}

#endif
