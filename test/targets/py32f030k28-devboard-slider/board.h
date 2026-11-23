// BRING-UP target: Embedfire PY32F030K28U6 dev board (QFN-32) as a slider
// module. The pot and bus pins follow a typical STM32G030 slider module; the
// status LED is the board's own. QFN-32 pin numbers from the PY32F030 datasheet V2.8, table 3-2.
//
// Wiring (see docs/py32-porting.md, "Dev-board bring-up"):
//   Jacdac data --[220R]-- PB6 (pin 29)
//   Jacdac GND ----------- GND
//   Power the board from its own USB; do NOT also feed it from JD_PWR.
//   Potentiometer (any value from 1 k to ~100 k): wiper to PA0 (pin 6), one end
//   to PB7 (pin 30, driven high only while sampling), the other end to GND.
//   Status LED: the board's own LED4 (blue) on PA4, active low. The usual
//   R/G/B status pins can't be used on this board: PA6 is one of its keys.
#define PIN_LED PA_4
#define PIN_LED_ACTIVE_LO 1
#define PIN_LED_GND -1

#define PIN_PWR NO_PIN
#define PIN_P0 NO_PIN
#define PIN_P1 NO_PIN

#define UART_PIN PB_6
#define UART_PIN_AF LL_GPIO_AF_0
#define USART_IDX 1

#define ADC_PIN PA_0
#define POT_EN PB_7

// Bootloader LED: LED4 on PA4 (TIM14_CH1 @ AF4), driven active low.
#define PIN_BL_LED PA_4
#define PIN_BL_PERIOD 300

// A G0 module's BOARD_STARTUP_CODE (PA11/PA12 pin remap) has no PY32
// equivalent and isn't needed here.
