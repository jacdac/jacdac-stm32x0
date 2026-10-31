#include "bl.h"

#ifdef USART_ISR_TXE_TXFNF
#define USART_ISR_TXE USART_ISR_TXE_TXFNF
#define USART_ISR_RXNE USART_ISR_RXNE_RXFNE
#endif

#define PORT(pin) ((GPIO_TypeDef *)(GPIOA_BASE + (0x400 * (pin >> 4))))
#define PIN(pin) (1 << ((pin)&0xf))

#define PIN_PORT PORT(UART_PIN)
#define PIN_PIN PIN(UART_PIN)
#define PIN_MODER (1 << (UART_PIN & 0xf) * 2)

#if USART_IDX == 1
#define USARTx USART1
#elif USART_IDX == 2
#define USARTx USART2
#else
#error "bad usart"
#endif

// Register access. PY32's USART is STM32F1-style (see py32f0.h): SR/DR, with no
// ISR, ICR, RDR/TDR, BUSY, REACK or TEACK, and errors clear by reading SR then DR.
//
// "Line busy" stands in for ST's USART_ISR_BUSY, which stays set while a frame is
// being received. The pin alone isn't enough: mid-frame it is high for most data
// and stop bits, so a bootloader that missed the frame's break would see an idle
// line and could start transmitting into it. So: busy if the pin is low (a break
// or a 0 bit), OR a byte has arrived and no idle line has been detected since
// (RXNE set, IDLE not yet set). IDLE rises one character time after the frame
// ends, so this can't stick. py32_usart_clear_errors() clears both flags.
#if defined(PY32F0)
#define UART_STATUS() (USARTx->SR)
#define UART_FE USART_SR_FE
#define UART_RXNE USART_SR_RXNE
#define UART_TXE USART_SR_TXE
#define UART_READ() (USARTx->DR)
#define UART_WRITE(b) (USARTx->DR = (b))
#define UART_LINE_BUSY(isr)                                                                        \
    (!(PIN_PORT->IDR & PIN_PIN) || (((isr) & (USART_SR_RXNE | USART_SR_IDLE)) == USART_SR_RXNE))
#define UART_CLEAR_ERRORS() py32_usart_clear_errors(USARTx)
#define UART_WAIT_REACK() ((void)0)
#define UART_WAIT_TEACK() ((void)0)
#else
#define UART_STATUS() (USARTx->ISR)
#define UART_FE USART_ISR_FE
#define UART_RXNE USART_ISR_RXNE
#define UART_TXE USART_ISR_TXE
#define UART_READ() (USARTx->RDR)
#define UART_WRITE(b) (USARTx->TDR = (b))
#define UART_LINE_BUSY(isr) ((isr)&USART_ISR_BUSY)
#define UART_CLEAR_ERRORS() (USARTx->ICR = USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)
#define UART_WAIT_REACK()                                                                          \
    while (!(LL_USART_IsActiveFlag_REACK(USARTx)))                                                 \
        ;
#define UART_WAIT_TEACK()                                                                          \
    while (!(LL_USART_IsActiveFlag_TEACK(USARTx)))                                                 \
        ;
#endif

// This is extracted to a function to make sure the compiler doesn't
// insert stuff between checking the input pin and setting the mode.
// This results in a collision window of around 90ns
__attribute__((noinline)) void gpio_probe_and_set(GPIO_TypeDef *gpio, uint32_t pin, uint32_t mode) {
    if (gpio->IDR & pin)
        gpio->MODER = mode;
}

static void uartOwnsPin(void) {
    LL_GPIO_SetPinMode(PIN_PORT, PIN_PIN, LL_GPIO_MODE_ALTERNATE);
    if ((UART_PIN & 0xf) <= 7)
        LL_GPIO_SetAFPin_0_7(PIN_PORT, PIN_PIN, UART_PIN_AF);
    else
        LL_GPIO_SetAFPin_8_15(PIN_PORT, PIN_PIN, UART_PIN_AF);
}

static void uartDoesntOwnPin(void) {
    LL_GPIO_SetPinMode(PIN_PORT, PIN_PIN, LL_GPIO_MODE_INPUT);
}

static void rx_setup(void) {
    uartOwnsPin();
    LL_USART_DisableDirectionTx(USARTx);
    LL_USART_EnableDirectionRx(USARTx);
    UART_CLEAR_ERRORS(); // clear error flags before we start
    LL_USART_Enable(USARTx);
    UART_WAIT_REACK();
}

void uart_init(ctx_t *ctx) {
#if QUICK_LOG == 1
    ctx->log_reg = (volatile uint32_t *)&PORT(PIN_X0)->BSRR;
    ctx->log_p0 = PIN(PIN_X0);
    ctx->log_p1 = PIN(PIN_X1);
#endif

    LL_GPIO_SetPinPull(PIN_PORT, PIN_PIN, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinSpeed(PIN_PORT, PIN_PIN, LL_GPIO_SPEED_FREQ_HIGH);
    uartDoesntOwnPin();

#if defined(PY32F0)
    // 16x oversampling: PY32 keeps OVER8 in CR3 (bit 11), so ORing
    // LL_USART_OVERSAMPLING_8 into CR1 would set WAKE instead and leave the bus at
    // half speed. BRR = CPU_MHZ (48) is exactly 1 Mbaud at 16x.
    USARTx->CR1 = LL_USART_DATAWIDTH_8B | LL_USART_PARITY_NONE | LL_USART_DIRECTION_TX;
    USARTx->BRR = CPU_MHZ; // ->1MHz (16x oversampling)
#else
    USARTx->CR1 = LL_USART_DATAWIDTH_8B | LL_USART_PARITY_NONE | LL_USART_OVERSAMPLING_8 |
                  LL_USART_DIRECTION_TX;
    USARTx->BRR = CPU_MHZ * 2; // ->1MHz
#endif

#ifdef LL_USART_PRESCALER_DIV1
    LL_USART_SetPrescaler(USARTx, LL_USART_PRESCALER_DIV1);
#endif

    LL_USART_ConfigHalfDuplexMode(USARTx);

    rx_setup();
}

void uart_disable(ctx_t *ctx) {
    LL_USART_Disable(USARTx);
    uartDoesntOwnPin();
}

int uart_rx(ctx_t *ctx, void *data, uint32_t maxbytes) {
    uint32_t isr = UART_STATUS();

    if (!ctx->low_detected && !(isr & UART_FE)) {
        if (UART_LINE_BUSY(isr))
            return RX_LINE_BUSY;
        return RX_LINE_IDLE;
    }

    ctx->low_detected = 0;

    // wait for BUSY to be cleared - ie the low pulse has ended
    while (UART_LINE_BUSY(isr))
        isr = UART_STATUS();

    uart_post_rx(ctx); // clear errors

    uint8_t *dp = data;
    int32_t delaycnt = -10 * CPU_MHZ; // allow a bit more delay at the start
    LOG0_PULSE();
    while (maxbytes > 0) {
        isr = UART_STATUS();
#if defined(PY32F0)
        // The next frame's break sets FE and RXNE together, and on F1-style parts
        // reading DR after this SR read clears FE. Test FE first, or the break is
        // stored as a data byte and the loop runs on to the timeout (ST keeps FE
        // until ICR is written, so it exits on the next pass).
        if (isr & UART_FE)
            break;
#endif
        if (isr & UART_RXNE) {
            *dp++ = UART_READ();
            maxbytes--;
            delaycnt = 0;
        } else if (isr & UART_FE || delaycnt > 4 * CPU_MHZ) {
            break;
        } else {
            delaycnt++;
        }
    }
    LOG0_PULSE();

    return RX_RECEPTION_OK;
}

void uart_post_rx(ctx_t *ctx) {
    (void)UART_READ();
    UART_CLEAR_ERRORS();
    (void)UART_READ();
}

int uart_tx(ctx_t *ctx, const void *data, uint32_t numbytes) {
    LL_USART_Disable(USARTx);
    uartDoesntOwnPin();

    LL_GPIO_ResetOutputPin(PIN_PORT, PIN_PIN);
    gpio_probe_and_set(PIN_PORT, PIN_PIN, PIN_MODER | PIN_PORT->MODER);
    if (!(PIN_PORT->MODER & PIN_MODER)) {
        ctx->low_detected = 1;
        rx_setup();
        return -1;
    }

    target_wait_us(11);
    LL_GPIO_SetOutputPin(PIN_PORT, PIN_PIN);

    uartOwnsPin();
    LL_USART_DisableDirectionRx(USARTx);
    LL_USART_EnableDirectionTx(USARTx);
    UART_CLEAR_ERRORS(); // clear error flags before we start
    LL_USART_Enable(USARTx);
    UART_WAIT_TEACK();

    const uint8_t *sp = data;
    target_wait_us(40);

    while (numbytes > 0) {
        uint32_t isr = UART_STATUS();
        if (isr & UART_TXE) {
            numbytes--;
            UART_WRITE(*sp++);
        }
    }

    while (!LL_USART_IsActiveFlag_TC(USARTx))
        ;

    LL_USART_Disable(USARTx);
    LL_GPIO_SetPinMode(PIN_PORT, PIN_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_ResetOutputPin(PIN_PORT, PIN_PIN);
    target_wait_us(12);
    LL_GPIO_SetOutputPin(PIN_PORT, PIN_PIN);
    rx_setup();

    return 0;
}
