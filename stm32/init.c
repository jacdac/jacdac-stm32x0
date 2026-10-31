#include "jdstm.h"

void tim_update_prescaler(void);

void HardFault_Handler(void) {
    JD_PANIC();
}

#ifndef BL
void NMI_Handler(void) {}

void SVC_Handler(void) {}

void PendSV_Handler(void) {}

void SysTick_Handler(void) {}
#endif

#define OPTR_MODE0 0x1
#define OPTR_MODE1 0x2
#define OPTR_MODE_LEGACY 0x3

// PY32: the option-byte rewrite below is skipped. PY32's NRST_MODE is a single
// bit (OPTR bit 14, vs two on G0) and Puya's option-byte programming sequence
// differs; getting either wrong here would rewrite option bytes on every boot, or
// disable the reset pin. PY32 parts keep their factory default (PF2 as NRST; verify
// on hardware). Fail loudly rather than silently ignore a board asking for GPIO.
#if defined(PY32F0) && defined(RESET_AS_GPIO)
#error "RESET_AS_GPIO is not implemented on PY32 yet (see enable_nrst_pin in init.c)"
#endif

static void enable_nrst_pin(void) {
#if defined(FLASH_OPTR_NRST_MODE) && !defined(PY32F0)
#define FLASH_KEY1 0x45670123U
#define FLASH_KEY2 0xCDEF89ABU

    int msk = (FLASH->OPTR & FLASH_OPTR_NRST_MODE) >> FLASH_OPTR_NRST_MODE_Pos;

#ifdef RESET_AS_GPIO
    // Reset as normal GPIO
    if (msk == OPTR_MODE1)
        return;
#else
    // Reset as conventional reset line
    if (msk == OPTR_MODE0 || msk == OPTR_MODE_LEGACY)
        return;
#endif

    uint32_t nrstmode;

    /* Enable Flash access anyway */
    __HAL_RCC_FLASH_CLK_ENABLE();

    // check no flash op is ongoing
    while ((FLASH->SR & FLASH_SR_BSY1) != 0)
        ;

    /* Unlock flash */
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
    while ((FLASH->CR & FLASH_CR_LOCK) != 0x00)
        ;

    /* unlock option byte registers */
    FLASH->OPTKEYR = 0x08192A3B;
    FLASH->OPTKEYR = 0x4C5D6E7F;
    while ((FLASH->CR & FLASH_CR_OPTLOCK) == FLASH_CR_OPTLOCK)
        ;

    /* get current user option bytes */
    nrstmode = (FLASH->OPTR & ~FLASH_OPTR_NRST_MODE);

#ifdef RESET_AS_GPIO
    nrstmode |= FLASH_OPTR_NRST_MODE_1;
#else
    nrstmode |= FLASH_OPTR_NRST_MODE_0;
#endif

    /* Program option bytes */
    FLASH->OPTR = nrstmode;
    while ((FLASH->SR & FLASH_SR_BSY1) != 0)
        ;
    // store options via options start
    FLASH->CR |= FLASH_CR_OPTSTRT;
    // wait for complete
    while ((FLASH->SR & FLASH_SR_BSY1) != 0)
        ;

    /* Force OB Load */
    FLASH->CR |= FLASH_CR_OBL_LAUNCH;

    // OBL triggers a reset
    while (1)
        ;
#endif
}

bool clk_is_pll(void) {
#if defined(STM32C0)
    // C0 has no PLL; it always runs directly off HSI48.
    return false;
#elif defined(PY32F0)
#if PY32_HAS_PLL
    // The F030 runs from its PLL (fixed x2). GetSysClkSource() returns the SWS
    // *status* bits, so compare with the STATUS constant; the SW *selector*
    // constant (used by the #else below) never matches. That comparison is also
    // wrong on ST parts, but nothing calls this today.
    return LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_STATUS_PLL;
#else
    // F003/F002A have no PLL (and no PLL constants in Puya's headers).
    return false;
#endif
#else
    return LL_RCC_GetSysClkSource() == LL_RCC_SYS_CLKSOURCE_PLL;
#endif
}

void clk_setup_pll(void) {
#if defined(STM32C0) || defined(PY32F0)
    // C0 has no PLL, and PY32 runs its PLL at a fixed x2 that is never switched at
    // runtime: in both cases the 48 MHz system clock is set up once in SystemInit().
#else
#if defined(STM32G0)
    // run at 48 MHz for compatibility with F0
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 6, LL_RCC_PLLR_DIV_2);
#elif defined(STM32F0)
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);

#if defined(STM32F042x6)
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLL_MUL_12, LL_RCC_PREDIV_DIV_2);
#else
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2, LL_RCC_PLL_MUL_12);
#endif
#elif defined(STM32L)
    // run at 48MHz
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 6, LL_RCC_PLLR_DIV_2);
#else
#error "clock?"
#endif

    LL_FLASH_EnablePrefetch();

    LL_RCC_PLL_Enable();
#ifdef RCC_PLLCFGR_PLLREN
    LL_RCC_PLL_EnableDomain_SYS();
#endif
    while (LL_RCC_PLL_IsReady() != 1)
        ;

    // LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
        ;
#endif // !STM32C0
}

#if defined(PY32F0)
// Puya's GCC startup (from their Embedded IDE templates) branches to `entry`
// where ST's branches to `main`. The vendored startup is kept byte-for-byte, so
// provide the symbol here; init.c is linked into both the app and the bootloader.
int main(void);
void entry(void) {
    main();
}
#endif

uint8_t cpu_mhz = HSI_MHZ;

#if JD_LORA || defined(STM32L4)
uint32_t SystemCoreClock = HSI_MHZ * 1000000;
#define SET_MHZ(n)                                                                                 \
    cpu_mhz = (n);                                                                                 \
    SystemCoreClock = (n)*1000000
#else
#define SET_MHZ(n) cpu_mhz = n
#endif

void clk_set_pll(int on) {
#if defined(STM32L4)
    SET_MHZ(PLL_MHZ);
    tim_update_prescaler();
#elif defined(STM32L) || !defined(DISABLE_PLL)
    if (!on) {
        LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
        while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
            ;
        SET_MHZ(HSI_MHZ);
        LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
        LL_FLASH_DisablePrefetch();
        tim_update_prescaler();
        return;
    }

    clk_setup_pll();

    SET_MHZ(PLL_MHZ);
    tim_update_prescaler();

    // LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
#endif
}

void SystemInit(void) {
    // on G0 this is called before global variables are initialized
    // also, they will be initialized after this is finished, so any writes to globals will be lost

#ifdef STM32G0
    SCB->VTOR = FLASH_BASE; // needed?
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA | LL_IOP_GRP1_PERIPH_GPIOB |
                            LL_IOP_GRP1_PERIPH_GPIOC);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
#elif defined(STM32C0)
    SCB->VTOR = FLASH_BASE; // needed?
    // C0's SYSCFG lives on APB1 group 2 (RCC_APBENR2), unlike G0's APB2 group 1.
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA | LL_IOP_GRP1_PERIPH_GPIOB |
                            LL_IOP_GRP1_PERIPH_GPIOC);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    // C0 has no PLL: it boots on HSI48 but with HSIDIV at reset (= /4 => 12 MHz).
    // Bring the system clock to the full 48 MHz by running HSI48 undivided. Raise
    // flash latency first (1 wait state above 24 MHz), then set HSIDIV to /1. HSI
    // is already the active system clock out of reset.
    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1)
        ;
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_1)
        ;
    LL_FLASH_EnablePrefetch();
    LL_RCC_SetHSIDiv(LL_RCC_HSI_DIV_1);
#elif defined(PY32F0)
    SCB->VTOR = FLASH_BASE;
    // PY32's SYSCFG and PWR sit on APB1 group 2 and group 1 respectively; GPIO is
    // on the IOP bus as on G0/C0. There is no GPIOC; the third port is GPIOF.
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA | LL_IOP_GRP1_PERIPH_GPIOB |
                            LL_IOP_GRP1_PERIPH_GPIOF);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    // PY32 boots on HSI at 8 MHz. Target: a fixed HSI_MHZ, i.e. 48 MHz on the
    // F030 (HSI 24 MHz x the PLL's fixed x2) or 24 MHz on the F003/F002A (HSI
    // 24 MHz directly; no PLL). See py32f0.h for why it never changes at runtime.
    // 1. HSI to 24 MHz, with Puya's factory trim for that setting. The macro
    //    reads the trim from the info block at 0x1FFF0F10.
    LL_RCC_HSI_SetCalibFreq(LL_RCC_HSICALIBRATION_24MHz);
    while (LL_RCC_HSI_IsReady() != 1)
        ;
    // 2. AHB and APB undivided: the USART is clocked from PCLK, and BRR = HSI_MHZ
    //    assumes PCLK equals the system clock.
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
#if PY32_HAS_PLL
    // 3. One flash wait state is required above 24 MHz; set it BEFORE raising the
    //    clock. (PY32 has no prefetch control, so no EnablePrefetch here.)
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_1)
        ;
    // 4. PLL from HSI (the x2 is fixed), then switch SYSCLK to it.
    LL_RCC_PLL_SetMainSource(LL_RCC_PLLSOURCE_HSI);
    LL_RCC_PLL_Enable();
    while (LL_RCC_PLL_IsReady() != 1)
        ;
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
        ;
#else
    // 3. 24 MHz needs no flash wait state. SYSCLK is already HSISYS out of reset
    //    and HSI is now at 24 MHz, so there is nothing to switch.
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
#endif
#elif defined(STM32L)
    SCB->VTOR = FLASH_BASE; // needed?
    // LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG); - doesn't have?
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA | LL_AHB2_GRP1_PERIPH_GPIOB |
                             LL_AHB2_GRP1_PERIPH_GPIOC);
#ifdef STM32L4
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
#ifdef LL_APB1_GRP1_PERIPH_RTCAPB
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_RTCAPB);
#endif
#endif

#ifdef STM32L4
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2)
        ;
    LL_FLASH_EnablePrefetch();

    // needed for I2C/UART, at least for now
    LL_RCC_HSI_Enable();
    while (!LL_RCC_HSI_IsReady())
        ;

    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_RCC_MSI_Enable();
    while (LL_RCC_MSI_IsReady() == 0U)
        ;
    LL_RCC_MSI_EnableRangeSelection();
    LL_RCC_MSI_SetRange(LL_RCC_MSIRANGE_11); // 48MHz
    LL_RCC_MSI_SetCalibTrimming(0);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_MSI);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_MSI)
        ;
    clk_set_pll(0);

#else
    // by default MSI is used not HSI16
    LL_RCC_HSI_Enable();
    while (!LL_RCC_HSI_IsReady())
        ;
    clk_set_pll(0);
    LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI);
#endif

#else // STM32F0
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA | LL_AHB1_GRP1_PERIPH_GPIOB |
                             LL_AHB1_GRP1_PERIPH_GPIOC | LL_AHB1_GRP1_PERIPH_GPIOF);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
#endif

#ifdef STM32L4
    // enable FPU
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
#endif

#ifdef BOARD_STARTUP_CODE
    BOARD_STARTUP_CODE;
#endif
    enable_nrst_pin();
}
