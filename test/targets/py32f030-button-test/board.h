// SCRATCH compile-test target: button on PY32F030F28P6TU, "F2" pinout.
#define PIN_LED NO_PIN
#define PIN_LED_GND -1

#define PIN_PWR -1
#define PIN_P0 NO_PIN
#define PIN_P1 NO_PIN

// Jacdac bus: USART1_TX on PB6 @ AF0.
#define UART_PIN PB_6
#define UART_PIN_AF LL_GPIO_AF_0
#define USART_IDX 1

// Status RGB: PA6/PA7 on TIM3, blue on PA4/TIM14 (PB1 unbonded).
#define PIN_LED_R PA_6
#define PIN_LED_G PA_7
#define PIN_LED_B PA_4

#define LED_R_MULT 250
#define LED_G_MULT 250
#define LED_B_MULT 221
#define RGB_LED_PERIOD 600

#define PIN_BL_LED PIN_LED_B
#define PIN_BL_PERIOD 300
