# Puya PY32F0 family (PY32F030 / PY32F003 / PY32F002A). See py32/py32f0.h.
#
# Everything PY32-only lives in py32/ (Puya's LL, CMSIS, this file, the per-MCU
# files and py32f0.h). The shared drivers in stm32/ carry the PY32 branches.
# A target's config.mk includes this file: include $(JD_STM)/py32/mk/py32f0.mk
PY32_DIR = $(JD_STM)/py32
MCU_MK = $(PY32_DIR)/mk/$(MCU).mk
CONFIG_DEPS += $(wildcard $(PY32_DIR)/*.h $(PY32_DIR)/mk/*.mk)
#
# stm32.mk adds -DSTM32$(SERIES), which gives an unused -DSTM32PY32F0 here; the
# family flag the code actually tests is the explicit -DPY32F0 below.
SERIES = PY32F0
CFLAGS += -mcpu=cortex-m0plus
DEFINES += -DPY32F0

# ST's CubeProgrammer won't flash PY32. Use Puya's OpenOCD build, pyOCD, J-Link
# or a DAP probe; see tools/README.md.
OPENOCD ?= openocd -f interface/cmsis-dap.cfg -f target/py32f0x.cfg

HALPREF = $(PY32_DIR)/py32f0xx_ll_driver/Src
HALSRC =  \
$(HALPREF)/py32f0xx_ll_adc.c \
$(HALPREF)/py32f0xx_ll_dma.c \
$(HALPREF)/py32f0xx_ll_exti.c \
$(HALPREF)/py32f0xx_ll_gpio.c \
$(HALPREF)/py32f0xx_ll_i2c.c \
$(HALPREF)/py32f0xx_ll_pwr.c \
$(HALPREF)/py32f0xx_ll_rcc.c \
$(HALPREF)/py32f0xx_ll_rtc.c \
$(HALPREF)/py32f0xx_ll_spi.c \
$(HALPREF)/py32f0xx_ll_tim.c \
$(HALPREF)/py32f0xx_ll_usart.c \
$(HALPREF)/py32f0xx_ll_utils.c \

CPPFLAGS += 	\
	-I$(PY32_DIR) \
	-I$(PY32_DIR)/py32f0xx_ll_driver/Inc \
	-I$(PY32_DIR)/cmsis_device_py32f0/Include \
	-I$(PY32_DIR)/cmsis_core_py32/Include

include $(PLATFORM)/mk/stm32.mk
