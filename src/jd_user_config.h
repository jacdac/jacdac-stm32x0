#ifndef JD_USER_CONFIG_H
#define JD_USER_CONFIG_H


#include "pinnames.h"
#include "board.h"
#include "dmesg.h"

#ifndef JD_CONFIG_TEMPERATURE
#define JD_CONFIG_TEMPERATURE 1
#endif

#define JD_LOG DMESG

#ifndef PIN_LED_GND
#define PIN_LED_GND NO_PIN
#endif

// secondary LED
#ifndef PIN_LED2
#define PIN_LED2 NO_PIN
#endif

// logging pins for JD implementation
#ifndef PIN_LOG0
#define PIN_LOG0 NO_PIN // sig_write
#endif
#ifndef PIN_LOG1
#define PIN_LOG1 NO_PIN
#endif
#ifndef PIN_LOG2
#define PIN_LOG2 NO_PIN // sig error
#endif
#ifndef PIN_LOG3
#define PIN_LOG3 NO_PIN
#endif

// logging pins for application
#ifndef PIN_P0
#define PIN_P0 NO_PIN
#endif
#ifndef PIN_P1
#define PIN_P1 NO_PIN
#endif

#ifndef PIN_PWR
#define PIN_PWR NO_PIN
#endif

#if defined(JD_CLIENT) && JD_CLIENT
#define JD_GC_ALLOC 1
#define JD_HW_ALLOC 1
#endif

// Stack kept free above the heap (jacdac-c simple_alloc.c; its default is 512 B).
// The static worst case on PY32 (test/stack-usage.py, docs/py32-porting.md,
// "Stack") is up to 740 B with DMESG compiled out and about 1.6 KB with it in,
// both over 512 B. It matters once a service takes all the remaining heap, as
// the LED strip does. A board.h can still set its own value.
#if defined(PY32F0) && !defined(JD_STACK_SIZE)
#if defined(DEVICE_DMESG_BUFFER_SIZE) && DEVICE_DMESG_BUFFER_SIZE == 0
#define JD_STACK_SIZE 1024
#else
#define JD_STACK_SIZE 1792
#endif
#endif

#endif
