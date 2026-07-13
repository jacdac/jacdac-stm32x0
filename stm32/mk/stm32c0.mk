SERIES = C0
CFLAGS += -mcpu=cortex-m0plus

OPENOCD ?= openocd -f interface/cmsis-dap.cfg -f target/stm32c0x.cfg

HALPREF = $(PLATFORM)/stm32c0xx_hal_driver/Src
HALSRC =  \
$(HALPREF)/stm32c0xx_ll_adc.c \
$(HALPREF)/stm32c0xx_ll_crs.c \
$(HALPREF)/stm32c0xx_ll_dma.c \
$(HALPREF)/stm32c0xx_ll_exti.c \
$(HALPREF)/stm32c0xx_ll_gpio.c \
$(HALPREF)/stm32c0xx_ll_i2c.c \
$(HALPREF)/stm32c0xx_ll_pwr.c \
$(HALPREF)/stm32c0xx_ll_rcc.c \
$(HALPREF)/stm32c0xx_ll_rtc.c \
$(HALPREF)/stm32c0xx_ll_spi.c \
$(HALPREF)/stm32c0xx_ll_tim.c \
$(HALPREF)/stm32c0xx_ll_usart.c \
$(HALPREF)/stm32c0xx_ll_utils.c \

CPPFLAGS += 	\
	-I$(PLATFORM)/stm32c0xx_hal_driver/Inc \
	-I$(PLATFORM)/cmsis_device_c0/Include \
	-I$(PLATFORM)/cmsis_core/Include

include $(PLATFORM)/mk/stm32.mk
