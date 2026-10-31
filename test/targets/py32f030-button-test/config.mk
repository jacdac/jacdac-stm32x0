# SCRATCH compile-test target: a button module on PY32F030F28P6TU.
MCU = PY32F030x8
include $(JD_STM)/py32/mk/py32f0.mk
# 4 KB RAM recipe (docs/py32-porting.md, "RAM"): drop both 1 KB debug log buffers
# (platform + jacdac-c) and the flood-ping bus-test command.
DEFINES += -DDEVICE_DMESG_BUFFER_SIZE=0 -DJD_CONFIG_CONTROL_FLOOD=0
