# PY32F003x6: 32 KB flash / 4 KB SRAM, e.g. PY32F003W16S6TU (SOP-16, "W1"
# pinout): the 4 KB RAM part. No PLL: runs at 24 MHz (see py32f0.h).
# On SOP-16, PB6 is not bonded: the Jacdac bus goes on PA2 (USART1_TX @ AF1).
RAM_SIZE = 4
FLASH_SIZE ?= 32
BL_SIZE ?= 4
PAGE_SIZE = 128
STARTUP_FILE = $(PY32_DIR)/cmsis_device_py32f0/Source/gcc/startup_py32f003xx.s
