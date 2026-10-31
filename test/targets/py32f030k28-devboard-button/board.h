// BRING-UP target: Embedfire PY32F030K28U6 core board (EBF410454 V1R0, QFN-32),
// using the board's own keys and LED. See docs/py32-porting.md,
// "Dev-board bring-up".
//
// Onboard parts, found by reading the factory demo's GPIO setup over SWD:
//   LED4 (blue)    PA4, active low (anode to 3V3). The status and bootloader LED.
//   LED2 / LED3    PA2 / PA3, active low. Not used: no PY32 PWM entry yet.
//   KEY1 / KEY2    PA5 / PA6 (order unconfirmed), to GND: internal pull-ups.
//   LED1           power LED.
//
// Wiring: only the bus.
//   Jacdac data --[220R]-- PB6 (left header J3, 7th from top)
//   Jacdac GND ----------- GND
//   Power the board from its own USB-C OR the ST-Link's 3V3, not both, and do
//   NOT also feed it from JD_PWR.
#define PIN_LED PA_4
#define PIN_LED_ACTIVE_LO 1
#define PIN_LED_GND -1

#define PIN_PWR -1
#define PIN_P0 NO_PIN
#define PIN_P1 NO_PIN

// Jacdac bus: USART1_TX on PB6 @ AF0, as on typical STM32G030 Jacdac modules. (PA2 @ AF1 also
// works on this board; it drives an onboard LED, which flickers with traffic.)
#define UART_PIN PB_6
#define UART_PIN_AF LL_GPIO_AF_0
#define USART_IDX 1

// The bootloader blinks this while waiting / flashing (TIM14_CH1 @ AF4). The
// bootloader drives it active low unless LED_RGB_COMMON_CATHODE is defined.
#define PIN_BL_LED PA_4
#define PIN_BL_PERIOD 300

#define KEY_A PA_5
#define KEY_B PA_6
