#!/usr/bin/env bash
# Flash a built target onto a PY32 over SWD, using pyOCD and an ST-Link (or any
# CMSIS-DAP / J-Link probe pyOCD supports).
#
# STM32CubeProgrammer won't flash PY32 parts, and mainline OpenOCD has no PY32
# flash driver, so this uses pyOCD with the flash algorithm from Puya's own CMSIS
# device pack (tools/README.md).
#
# Usage:  tools/flash.sh <target> [--erase]
#   e.g.  tools/flash.sh py32f030k28-devboard-button --erase
#
# Build first with test/build-check.sh; this flashes
# built/<target>/combined-*.hex (bootloader + app), which is what a blank part
# needs. After that, updates can go over the Jacdac bus as UF2 instead.
# --erase does a full chip erase first (use it for the very first flash).
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
t="${1:?usage: $0 <target> [--erase]}"
erase=""
[ "${2:-}" = "--erase" ] && erase="--erase chip"

# '|| true': under set -e a failed ls would end the script before the message below
pack="$(ls "$here"/tools/Puya.PY32F0xx_DFP.*.pack 2> /dev/null | tail -1 || true)"
if [ -z "$pack" ]; then
    echo "error: no Puya device pack in tools/ (see tools/README.md)" >&2
    exit 1
fi

cfg="$here/test/targets/$t/config.mk"
if [ ! -f "$cfg" ]; then
    echo "error: no target $t in test/targets/" >&2
    exit 1
fi
# pyOCD's target name is the MCU from config.mk, lower-cased (PY32F030x8 -> py32f030x8)
mcu="$(sed -n 's/^MCU *= *//p' "$cfg" | tr -d '[:space:]' | tr '[:upper:]' '[:lower:]')"

hex="$(ls "$here/built/$t/"combined-*.hex 2> /dev/null | head -1 || true)"
if [ -z "$hex" ]; then
    echo "error: nothing built for $t; run: test/build-check.sh $t" >&2
    exit 1
fi

# Pick the probe explicitly. A micro:bit on USB is a CMSIS-DAP probe too, and
# with both plugged in pyOCD grabs whichever it finds first.
# PY32_PROBE=<unique id> overrides; otherwise use the one ST-Link plugged in.
probe="${PY32_PROBE:-}"
if [ -z "$probe" ]; then
    stlinks="$(pyocd list 2> /dev/null | grep -i 'stlink\|st-link' | grep -oE '[0-9A-Fa-f]{24}' || true)"
    count="$(printf '%s' "$stlinks" | grep -c . || true)"
    if [ "$count" != "1" ]; then
        echo "error: expected one ST-Link, found $count (see: pyocd list);" >&2
        echo "       set PY32_PROBE=<unique id> to choose the probe" >&2
        exit 1
    fi
    probe="$stlinks"
fi

echo "flashing $hex"
echo "  target $mcu, probe $probe, pack $(basename "$pack")"
# shellcheck disable=SC2086
# --no-wait: fail straight away if the probe isn't plugged in, instead of hanging
pyocd flash --no-wait -u "$probe" --pack "$pack" --target "$mcu" $erase "$hex"
