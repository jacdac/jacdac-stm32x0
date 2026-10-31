# BRING-UP target for the PY32F030K28U6 dev board (QFN-32, 64 KB / 8 KB): a slider
# (potentiometer) module, to check the ADC. Same pins as a typical STM32G030
# slider module, all of which the QFN-32 bonds.
MCU = PY32F030x8
include $(JD_STM)/py32/mk/py32f0.mk
# 4 KB RAM recipe (docs/py32-porting.md, "RAM").
DEFINES += -DDEVICE_DMESG_BUFFER_SIZE=0 -DJD_CONFIG_CONTROL_FLOOD=0
