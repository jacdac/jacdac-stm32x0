#ifndef __STM_H
#define __STM_H

#include "board.h"

#define STM32C0 1

#include "stm32c0xx_ll_crc.h"
#include "stm32c0xx_ll_bus.h"
#include "stm32c0xx_ll_gpio.h"
#include "stm32c0xx_ll_exti.h"
#include "stm32c0xx_ll_dma.h"
#include "stm32c0xx_ll_rcc.h"
#include "stm32c0xx_ll_system.h"
#include "stm32c0xx_ll_cortex.h"
#include "stm32c0xx_ll_utils.h"
#include "stm32c0xx_ll_pwr.h"
#include "stm32c0xx_ll_usart.h"
#include "stm32c0xx_ll_tim.h"
#include "stm32c0xx_ll_spi.h"
#include "stm32c0xx_ll_adc.h"
#include "stm32c0xx_ll_rtc.h"
#include "stm32c0xx_ll_i2c.h"

#include "stm32c0xx_hal_rcc.h"

// C0 runs HSI48 directly as the system clock (with HSIDIV=1 => 48 MHz).
#define HSI_MHZ 48

// C0 has no PLL. PLL_MHZ is unused once the PLL is disabled, but keep it
// defined for parity with the other families.
#define PLL_MHZ 48

// C0 has no PLL at all, so always run the no-PLL clock path. The 48 MHz
// HSISYS is set up directly in SystemInit() (see init.c, STM32C0 branch).
#define DISABLE_PLL 1

#endif
