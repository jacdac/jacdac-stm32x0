# Button module on PY32F003W16S6TU (SOP-16, 32 KB / 4 KB, 24 MHz).
MCU = PY32F003x6
include $(JD_STM)/py32/mk/py32f0.mk
# 4 KB RAM recipe (docs/py32-porting.md, "RAM").
DEFINES += -DDEVICE_DMESG_BUFFER_SIZE=0 -DJD_CONFIG_CONTROL_FLOOD=0
