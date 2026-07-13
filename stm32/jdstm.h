#ifndef __JDSTM_H
#define __JDSTM_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#if defined(STM32F0)
#include "stm32f0.h"
#elif defined(STM32G0)
#include "stm32g0.h"
#elif defined(STM32C0)
#include "stm32c0.h"
#elif defined(STM32WL)
#include "stm32wl.h"
#elif defined(STM32L4)
#include "stm32l4.h"
#else
#error "invalid CPU"
#endif

#include "lib.h"

#define IRQ_PRIORITY_LORA 3
#define IRQ_PRIORITY_TIM 2
#define IRQ_PRIORITY_DMA 1
#define IRQ_PRIORITY_EXTI 0
#define IRQ_PRIORITY_UART 0

extern uint8_t cpu_mhz;

#define exti_disable(pin) LL_EXTI_DisableIT_0_31(pin)
#define exti_enable(pin) LL_EXTI_EnableIT_0_31(pin)
#define exti_enable(pin) LL_EXTI_EnableIT_0_31(pin)
#define exti_clear_falling(pin) LL_EXTI_ClearFallingFlag_0_31(pin)

void tim_set_micros(uint64_t us);
cb_t tim_steal_callback(uint32_t *usec);
void rtc_sync_time(void);
void rtc_cancel_cb(void);

void usb_init(void);

#define PIN_PORT(pin) ((GPIO_TypeDef *)(GPIOA_BASE + (0x400 * (pin >> 4))))
#define PIN_MASK(pin) (1 << ((pin)&0xf))

#ifndef RTC_MIN_TIME_US
#define RTC_MIN_TIME_US 500
#endif

#ifndef SPI_RX
#define SPI_RX 0
#endif

#ifdef __HAL_RCC_DMAMUX1_CLK_ENABLE
#define DMA_CLK_ENABLE()                                                                           \
    __HAL_RCC_DMA1_CLK_ENABLE();                                                                   \
    __HAL_RCC_DMAMUX1_CLK_ENABLE()
#else
#define DMA_CLK_ENABLE() __HAL_RCC_DMA1_CLK_ENABLE()
#endif

#define DMA_FLAG_G DMA_ISR_GIF1
#define DMA_FLAG_TC DMA_ISR_TCIF1
#define DMA_FLAG_HT DMA_ISR_HTIF1
#define DMA_FLAG_TE DMA_ISR_TEIF1

// The DMA ISR/IFCR layout is the same across these families (4 flag bits per
// channel), but the LL_DMA_CHANNEL_x constants differ: G0/L are 0-based
// (LL_DMA_CHANNEL_1 == 0) so the flag shifts by ch*4, while F0 and C0 are
// 1-based (LL_DMA_CHANNEL_1 == 1) and must shift by (ch-1)*4. C0 therefore
// belongs in the #else branch below with F0 (NOT with G0) — placing it with G0
// reads/clears the wrong channel's flags and hangs on the first DMA IRQ.
#if defined(STM32G0) || defined(STM32L)
static inline void dma_clear_flag(int ch, int flag) {
    WRITE_REG(DMA1->IFCR, flag << (ch*4));
}

static inline bool dma_has_flag(int ch, int flag) {
    return (READ_BIT(DMA1->ISR, flag << (ch*4)) != 0);
}
#else
static inline void dma_clear_flag(int ch, int flag) {
    WRITE_REG(DMA1->IFCR, flag << ((ch - 1) * 4));
}

static inline bool dma_has_flag(int ch, int flag) {
    return (READ_BIT(DMA1->ISR, flag << ((ch - 1) * 4)) != 0);
}
#endif

#endif
