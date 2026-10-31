# PY32F003x8: 64 KB flash / 8 KB SRAM, e.g. PY32F003W18S6TU (SOP-16, "W1"
# pinout). Same package and clock as the x6 (24 MHz, no PLL), twice the memory.
RAM_SIZE = 8
FLASH_SIZE ?= 64
BL_SIZE ?= 4
PAGE_SIZE = 128
STARTUP_FILE = $(PY32_DIR)/cmsis_device_py32f0/Source/gcc/startup_py32f003xx.s
