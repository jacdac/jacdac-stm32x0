#include "jdstm.h"

static cb_t callbacks[16];

static void _check_line(int ln) {
#if defined(STM32F0) || defined(STM32L) || defined(PY32F0)
    LL_EXTI_ClearFlag_0_31(1 << ln);
#else
    LL_EXTI_ClearRisingFlag_0_31(1 << ln);
    LL_EXTI_ClearFallingFlag_0_31(1 << ln);
#endif
    callbacks[ln]();
}

#if defined(STM32G0) || defined(STM32C0)
#define EXTI_LINES() (EXTI->FPR1 | EXTI->RPR1)
#elif defined(STM32L)
#define EXTI_LINES() (EXTI->PR1)
#else
#define EXTI_LINES() (EXTI->PR)
#endif

#define check_line(ln)                                                                             \
    if (lines & (1 << (ln)))                                                                       \
    _check_line(ln)

#ifdef STM32L
#define SINGLE(name, ln)                                                                           \
    void name(void) {                                                                              \
        rtc_sync_time();                                                                           \
        uint32_t lines = EXTI_LINES();                                                             \
        check_line(ln);                                                                            \
    }
SINGLE(EXTI0_IRQHandler, 0)
SINGLE(EXTI1_IRQHandler, 1)
SINGLE(EXTI2_IRQHandler, 2)
SINGLE(EXTI3_IRQHandler, 3)
SINGLE(EXTI4_IRQHandler, 4)

void EXTI9_5_IRQHandler(void) {
    rtc_sync_time();
    uint32_t lines = EXTI_LINES();
    for (int i = 5; i <= 9; ++i)
        check_line(i);
}

void EXTI15_10_IRQHandler(void) {
    rtc_sync_time();
    uint32_t lines = EXTI_LINES();
    for (int i = 10; i <= 15; ++i)
        check_line(i);
}
#else
void EXTI0_1_IRQHandler(void) {
    rtc_sync_time();
    uint32_t lines = EXTI_LINES();
    check_line(0);
    check_line(1);
}

void EXTI2_3_IRQHandler(void) {
    rtc_sync_time();
    uint32_t lines = EXTI_LINES();
    check_line(2);
    check_line(3);
}

void EXTI4_15_IRQHandler(void) {
    rtc_sync_time();

    uint32_t lines = EXTI_LINES();

#ifdef UART_PIN
    // first check UART line, to speed up handling
    if (lines & (1 << (UART_PIN & 0xf))) {
        _check_line(UART_PIN & 0xf);
        lines = EXTI_LINES();
    }
#endif

    // check the rest of the lines (this includes UART line, but it's fine)
    for (int i = 4; i <= 15; ++i) {
        check_line(i);
    }
}
#endif

#if defined(STM32G0) || defined(STM32C0)
static const uint32_t cfgs[] = {LL_EXTI_CONFIG_PORTA, LL_EXTI_CONFIG_PORTB, LL_EXTI_CONFIG_PORTC};
#elif defined(PY32F0)
// PY32 routes a GPIO to an EXTI line through EXTI->EXTICR (like G0/C0, not F0's
// SYSCFG), but with its own encoding, and only lines 0-8 have a selector at all:
// lines 0-4 choose port A/B/F, lines 5-8 choose A/B, and lines 9-15 are wired to
// port A. Puya's LL_EXTI_CONFIG_LINEx constants carry the register, position and
// mask for each selectable line.
static const uint32_t py32_exti_line[9] = {
    LL_EXTI_CONFIG_LINE0, LL_EXTI_CONFIG_LINE1, LL_EXTI_CONFIG_LINE2,
    LL_EXTI_CONFIG_LINE3, LL_EXTI_CONFIG_LINE4, LL_EXTI_CONFIG_LINE5,
    LL_EXTI_CONFIG_LINE6, LL_EXTI_CONFIG_LINE7, LL_EXTI_CONFIG_LINE8,
};
#endif

void exti_set_callback(uint8_t pin, cb_t callback, uint32_t flags) {
    uint32_t extiport = 0;

#if defined(PY32F0)
    // PY32 ports are A (0), B (1) and F (5: GPIOF sits at IOPORT_BASE + 0x1400).
    switch (pin >> 4) {
    case 0:
        extiport = LL_EXTI_CONFIG_PORTA;
        break;
    case 1:
        extiport = LL_EXTI_CONFIG_PORTB;
        break;
    case 5:
        extiport = LL_EXTI_CONFIG_PORTF;
        break;
    default:
        JD_PANIC();
    }
#else
    if (pin >> 4 > 2)
        JD_PANIC();
#endif
#if defined(PY32F0)
    // port already resolved above
#elif defined(STM32F0) || defined(STM32L)
    extiport = pin >> 4;
#elif defined(STM32G0) || defined(STM32C0)
    extiport = cfgs[pin >> 4];
#else
#error "exti?"
#endif

    int pos = pin & 0xf;

#ifdef STM32F0
    uint32_t line = (pos >> 2) | ((pos & 3) << 18);
    LL_SYSCFG_SetEXTISource(extiport, line);
#elif defined(STM32L)
    uint32_t line = (pos >> 2) | (0xF << (16 + (4 * (pos & 3))));
    LL_SYSCFG_SetEXTISource(extiport, line);
#elif defined(PY32F0)
    if (pos <= 8) {
        // lines 5-8 only select between A and B
        if (pos >= 5 && extiport == LL_EXTI_CONFIG_PORTF)
            JD_PANIC();
        LL_EXTI_SetEXTISource(extiport, py32_exti_line[pos]);
    } else if (extiport != LL_EXTI_CONFIG_PORTA) {
        // lines 9-15 have no selector and always come from port A
        JD_PANIC();
    }
#else
    uint32_t line = (pos >> 2) | ((pos & 3) << 19);
    LL_EXTI_SetEXTISource(extiport, line);
#endif
    if (flags & EXTI_FALLING)
        LL_EXTI_EnableFallingTrig_0_31(1 << pos);
    if (flags & EXTI_RISING)
        LL_EXTI_EnableRisingTrig_0_31(1 << pos);
    LL_EXTI_EnableIT_0_31(1 << pos);

    callbacks[pos] = callback;

#define SETUP(irq)                                                                                 \
    NVIC_SetPriority(irq, IRQ_PRIORITY_EXTI);                                                      \
    NVIC_EnableIRQ(irq)

#ifdef STM32L
    if (!NVIC_GetEnableIRQ(EXTI0_IRQn)) {
        SETUP(EXTI0_IRQn);
        SETUP(EXTI1_IRQn);
        SETUP(EXTI2_IRQn);
        SETUP(EXTI3_IRQn);
        SETUP(EXTI4_IRQn);
        SETUP(EXTI9_5_IRQn);
        SETUP(EXTI15_10_IRQn);
    }
#else
    if (!NVIC_GetEnableIRQ(EXTI0_1_IRQn)) {
        SETUP(EXTI0_1_IRQn);
        SETUP(EXTI2_3_IRQn);
        SETUP(EXTI4_15_IRQn);
    }
#endif
}