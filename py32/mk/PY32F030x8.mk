# PY32F030x8: 64 KB flash / 8 KB SRAM, e.g. PY32F030F28P6TU (TSSOP-20, "F2" pinout).
# PAGE_SIZE is the flash page (128 B on PY32, vs 2048 on G0/C0); the Jacdac
# bootloader announces it, so the host adapts (see docs/py32-flash.md).
RAM_SIZE = 8
FLASH_SIZE ?= 64
BL_SIZE ?= 4
PAGE_SIZE = 128
STARTUP_FILE = $(PY32_DIR)/cmsis_device_py32f0/Source/gcc/startup_py32f030xx.s
