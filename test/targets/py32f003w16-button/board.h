// Button module on PY32F003W16S6TU: SOP-16, "W1" pinout (PY32F003 datasheet
// V2.7, table 3-3; AF maps tables 3-6/3-7).
//
//   pin 1 VSS          pin 16 VCC
//   pin 3 PA13 SWDIO   pin 4  PA14 SWCLK, SHARED with PF1: never drive PF1
//   pin 5 PF2 NRST, SHARED with PF0
//   pin 6 PA0  button  pin 8  PA2  Jacdac bus
//   pin 12 PA6 LED R   pin 13 PA7  LED G   pin 15 PB1 LED B
#define PIN_LED NO_PIN
#define PIN_LED_GND -1

#define PIN_PWR -1
#define PIN_P0 NO_PIN
#define PIN_P1 NO_PIN

// Jacdac bus: PB6 isn't bonded on SOP-16, so USART1_TX on PA2 @ AF1.
#define UART_PIN PA_2
#define UART_PIN_AF LL_GPIO_AF_1
#define USART_IDX 1

// Status RGB: PA6/PA7 = TIM3_CH1/CH2 @ AF1, PB1 = TIM3_CH4 @ AF1 (as on typical
// STM32G030 Jacdac modules; PB1 is bonded on SOP-16).
#define PIN_LED_R PA_6
#define PIN_LED_G PA_7
#define PIN_LED_B PB_1

#define LED_R_MULT 250
#define LED_G_MULT 250
#define LED_B_MULT 221
#define RGB_LED_PERIOD 600

#define PIN_BL_LED PIN_LED_B
#define PIN_BL_PERIOD 300
