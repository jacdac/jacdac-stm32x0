# BRING-UP target for the PY32F030K28U6 dev board (QFN-32, 64 KB / 8 KB): the
# first thing to flash. A button module that should enumerate on a Jacdac bus
# next to other modules. Same memory as the TSSOP-20 PY32F030F28P6TU,
# so the same MCU file; only the pins differ.
MCU = PY32F030x8
include $(JD_STM)/py32/mk/py32f0.mk
# 4 KB RAM recipe (docs/py32-porting.md, "RAM").
DEFINES += -DDEVICE_DMESG_BUFFER_SIZE=0 -DJD_CONFIG_CONTROL_FLOOD=0
