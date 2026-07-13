RAM_SIZE = 12
FLASH_SIZE ?= 32
BL_SIZE ?= 4
PAGE_SIZE = 2048
DEFINES += -DSTM32C031xx
STARTUP_FILE = $(PLATFORM)/cmsis_device_c0/Source/Templates/gcc/startup_stm32c031xx.s
