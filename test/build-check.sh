#!/usr/bin/env bash
# Compile-check the PY32 family without any hardware.
#
# jacdac-py32 is a platform layer, not a standalone build (like upstream
# jacdac-stm32x0), so this assembles a throwaway top-level module repo in a temp
# directory: this repo in the platform slot, an upstream jacdac-c checkout, and the
# compile-test targets from test/targets/. Neither repo is modified.
#
# Usage:  test/build-check.sh [/path/to/jacdac-c] [target ...]
#
# With no path, it uses a jacdac-c clone next to this repo (../jacdac-c).
#
# Outputs (combined-*.hex for a first SWD flash, app-*.uf2 for updates over the
# bus) are copied to built/<target>/ in this repo, which git ignores.
#
# jacdac-c must be UPSTREAM jacdac/jacdac-c with its `jacdac` spec submodule
# checked out (verified with 89a08f3 / spec 47cb856). Older jacdac-c copies may
# not match what this platform layer expects.
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
if [ $# -gt 0 ] && [ -d "$1" ]; then
    jdc="$1"
    shift
else
    jdc="$here/../jacdac-c"
fi
if [ ! -d "$jdc" ]; then
    echo "error: no jacdac-c at $jdc; pass its path as the first argument" >&2
    exit 1
fi
jdc="$(cd "$jdc" && pwd)"
if [ ! -f "$jdc/jacdac/dist/c/control.h" ]; then
    echo "error: $jdc/jacdac is empty: check out jacdac-c's spec submodule" >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
ln -s "$here" "$work/jacdac-stm32x0"
ln -s "$jdc" "$work/jacdac-c"
cp -R "$here/test/targets" "$work/targets"
printf '.SECONDARY:\n.DEFAULT_GOAL := all\nJD_CORE = jacdac-c\nJD_STM = jacdac-stm32x0\n-include Makefile.user\ninclude $(JD_STM)/build.mk\n' > "$work/Makefile"
: > "$work/Makefile.user"
# the build stamps a firmware version from git
git -C "$work" init -q
git -C "$work" add -A > /dev/null 2>&1
git -C "$work" -c user.email=ci@local -c user.name=ci commit -qm check > /dev/null

if [ $# -gt 0 ]; then
    targets=("$@")
else
    targets=()
    for d in "$here"/test/targets/*/; do
        targets+=("$(basename "$d")")
    done
fi

status=0
for t in "${targets[@]}"; do
    if make -C "$work" TARGET="$t" > "$work/$t.log" 2>&1; then
        echo "OK    $t  ->  built/$t/"
        arm-none-eabi-size "$work/built/$t/app-"*.elf "$work/built/$t/bl-"*.elf | sed 's/^/      /'
        rm -rf "$here/built/$t"
        mkdir -p "$here/built"
        cp -R "$work/built/$t" "$here/built/$t"
        # the UF2 must parse the way the Jacdac host flasher parses it
        if ! node "$here/test/uf2-host-check.js" "$here/built/$t/"app-*.uf2 | sed 's/^/      uf2: /'; then
            status=1
        fi
    else
        echo "FAIL  $t"
        grep -E "error|undefined|oops" "$work/$t.log" | head -20 | sed 's/^/      /' || true
        status=1
    fi
done
exit $status
