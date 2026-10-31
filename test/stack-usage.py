#!/usr/bin/env python3
"""Worst-case stack depth of a built image, from GCC's own call-graph data.

Build with the extra flags, then point this at one image's object directory:

    USERFLAGS="-fstack-usage -fcallgraph-info=su" test/build-check.sh <target>
    python3 test/stack-usage.py built/<target>/app      # or .../bl

How the bound is computed (see docs/py32-porting.md, "Stack"):
  * Each function's own frame comes from -fstack-usage; call edges come from
    -fcallgraph-info. Only functions present in the final ELF are counted.
  * A call through a function pointer is charged as the worst of its REAL
    targets, from the INDIRECT table below (read from the source: timer and EXTI
    callbacks, service vtables, the event queue). A pointer call site missing
    from the table is WARNED about and charged as the worst address-taken
    function (R_ARM_ABS32 relocations), minus vtable/callback entries so the
    charge can't recurse. Either way the result is an upper bound.
  * Interrupts nest by priority: the worst case is main's deepest chain plus the
    deepest handler at each preemption level, plus a hardware exception frame per
    level (8 words, +4 for 8-byte alignment = 36 B on Cortex-M0+).
  * Library functions without stack data (libgcc/newlib) are charged a flat
    EXTERN_COST and listed.
  * Two bounds are printed: every path, and every path except calls into the
    panic handlers (PANIC below). A panic ends in a reset, so an overflow on a
    panic path only damages RAM that is about to be wiped; the second figure is
    the one normal operation has to fit.
"""
import glob
import os
import re
import subprocess
import sys
from collections import defaultdict

EXTERN_COST = 32   # per call into code with no -fstack-usage data (libgcc, newlib)
EXC_FRAME = 36     # Cortex-M0+ hardware stacking: 8 words + 4 B alignment pad
TOOL = "arm-none-eabi-"

# NVIC priorities as the platform code sets them (lower number = preempts higher):
#   uart.c   USART + its DMA IRQ   IRQ_PRIORITY_UART = 0
#   exti.c   EXTI lines            IRQ_PRIORITY_EXTI = 0
#   spidef.h SPI + its DMA IRQ     IRQ_PRIORITY_DMA  = 1
#   tim.c    system timer (TIM17)  IRQ_PRIORITY_TIM  = 2
PRIORITY = {
    "USART1_IRQHandler": 0, "USART2_IRQHandler": 0, "DMA1_Channel2_3_IRQHandler": 0,
    "EXTI0_1_IRQHandler": 0, "EXTI2_3_IRQHandler": 0, "EXTI4_15_IRQHandler": 0,
    "DMA1_Channel1_IRQHandler": 1, "SPI1_IRQHandler": 1,
    "TIM17_IRQHandler": 2,
}
# Targets of every function-pointer call site, keyed by caller (file:func for
# static functions). Names not linked into a given image are ignored.
TIMER_CB = ["flush_tx_queue", "tick", "rx_timeout", "setup_rx_timeout",  # jd_physical.c
            "do_nothing",                                               # src/main.c
            "disp_cb"]                                                  # stm32/display.c
EXTI_CB = ["jd_line_falling",                                           # stm32/uart.c
           "accelerometer_int"]                                         # services/accelerometer.c
INDIRECT = {
    "TIM17_IRQHandler": TIMER_CB,                         # tim.c: timer_cb()
    "jacdac-stm32x0/stm32/exti.c:_check_line": EXTI_CB,  # exti.c: callbacks[ln]()
    "jd_process_event_queue": ["do_process_event_queue"],  # event_queue.c: info.process()
    "jd_services_tick": "VT_PROCESS",                     # srv->vt->process(srv)
    "jd_services_handle_packet": "VT_HANDLE",             # srv->vt->handle_pkt(srv, pkt)
    # is_active is only set by button_init_fn(), which no target here uses
    "jacdac-c/services/button.c:update": [],
    "jacdac-c/services/switch.c:update": [],
    # WS2812 output (dspi.c): the DMA IRQ calls dma_handler (only ever px_dma), and
    # stop_dma() calls the px_tx() done handler (tx_done in ledstrip/leddisplay)
    "DMA1_Channel1_IRQHandler": ["px_dma"],
    "jacdac-stm32x0/stm32/dspi.c:stop_dma": ["tx_done"],
    # sensor driver API (env_sensor_api_t); resolved by the fallback
    "sensor_handle_packet": "FALLBACK", "sensor_maybe_init": "FALLBACK",
    "sensor_process": "FALLBACK",
}

# noreturn: JD_PANIC() / hw_panic() log (if DMESG is on) and reset the chip
PANIC = {"hw_panic", "jd_panic_core"}

IGNORE_ROOTS = {"Default_Handler", "HardFault_Handler", "NMI_Handler", "SVC_Handler",
                "PendSV_Handler", "SysTick_Handler", "Reset_Handler", "Boot_Handler"}

NODE = re.compile(r'node: \{ title: "([^"]+)" label: "([^"]*)"')
EDGE = re.compile(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"')
BYTES = re.compile(r"(\d+) bytes \(([^)]*)\)")


def main():
    objdir = sys.argv[1].rstrip("/")
    elf = find_elf(objdir)
    in_elf = elf_functions(elf)

    frame, qual, calls = {}, {}, defaultdict(set)
    for ci in glob.glob(objdir + "/**/*.ci", recursive=True):
        for line in open(ci, encoding="utf-8", errors="replace"):
            m = NODE.search(line)
            if m:
                title, label = m.groups()
                b = BYTES.search(label.replace("\\n", "\n"))
                if b:
                    frame[title] = int(b.group(1))
                    qual[title] = b.group(2)
                continue
            m = EDGE.search(line)
            if m:
                calls[m.group(1)].add(m.group(2))

    def fname(title):
        return title.rsplit(":", 1)[-1]

    def linked(title):
        n = fname(title)
        return n in in_elf or any(e.startswith(n + ".") for e in in_elf)

    live = {t for t in frame if linked(t)}
    # main, entry (stm32/init.c) and the handlers are address-taken only by the
    # vector table and startup code; nothing calls them through a pointer
    taken = {t for t in address_taken(objdir, in_elf, live)
             if fname(t) not in ("main", "entry") and not fname(t).endswith("Handler")}

    def titles(names):
        return {t for t in live if fname(t) in names}

    vt_process = {t for t in taken if fname(t).endswith("_process")}
    vt_handle = {t for t in taken if fname(t).endswith("_handle_packet")}
    callbacks = titles(set(TIMER_CB) | set(EXTI_CB) | {"do_process_event_queue"})
    fallback = taken - vt_process - vt_handle - callbacks
    unannotated = set()

    def targets(caller):
        spec = INDIRECT.get(caller, INDIRECT.get(fname(caller)))
        if spec is None:
            unannotated.add(caller)
            return fallback
        if spec == "VT_PROCESS":
            return vt_process
        if spec == "VT_HANDLE":
            return vt_handle
        if spec == "FALLBACK":
            return fallback
        return titles(set(spec))

    memo, stack, cycles, externs = {}, set(), set(), set()
    skip = set()

    def worst(t):
        if fname(t) in skip:
            return 0, []
        if t in memo:
            return memo[t]
        if t in stack:
            cycles.add(t)
            return 0, [t + " (recursion)"]
        if t not in frame:  # declared only: library code with no stack data
            externs.add(t)
            return EXTERN_COST, [t + " (extern, charged %d)" % EXTERN_COST]
        stack.add(t)
        best, path = 0, []
        for c in calls.get(t, ()):
            if c == "__indirect_call":
                d, p = 0, ["<indirect: no linked target>"]
                for x in targets(t):
                    dx, px = worst(x)
                    if dx > d:
                        d, p = dx, ["<indirect>"] + px
            else:
                d, p = worst(c)
            if d > best:
                best, path = d, p
        stack.discard(t)
        r = (frame[t] + best, [t] + path)
        memo[t] = r
        return r

    roots = sorted(t for t in live if fname(t) == "main" or
                   (fname(t).endswith("Handler") and fname(t) not in IGNORE_ROOTS))
    print("image: %s   (%d linked functions with stack data)" % (elf, len(live)))
    print("vtable targets: process %s; handle %s\n" % (
        sorted(map(fname, vt_process)), sorted(map(fname, vt_handle))))

    def bound(heading):
        memo.clear()
        print(heading)
        results = {}
        for r in roots:
            d, p = worst(r)
            results[fname(r)] = d
            print("  %-30s %5d B   %s" % (fname(r), d, " -> ".join(fname(x) for x in p[:8])))
        main_d = results.get("main", 0)
        levels = defaultdict(int)
        for n, d in results.items():
            if n != "main" and n in PRIORITY:
                levels[PRIORITY[n]] = max(levels[PRIORITY[n]], d)
        total = main_d + sum(d + EXC_FRAME for d in levels.values())
        print("  nesting: main %d B + %s" % (main_d, " + ".join(
            "prio%d %d+%d B" % (p, levels[p], EXC_FRAME) for p in sorted(levels, reverse=True))))
        return results, total

    results, total = bound("all paths:")
    skip.update(PANIC)
    _, normal = bound("\nexcluding panic paths (%s):" % ", ".join(sorted(PANIC)))
    print("\nWORST-CASE STACK (upper bound): %d B all paths, %d B excluding panic" % (total, normal))

    dyn = sorted(fname(t) for t in live if qual.get(t, "static") != "static")
    if dyn:
        print("\nfunctions with dynamic frames (size above is the static part): " + ", ".join(dyn))
    if cycles:
        print("recursion (not counted beyond one level): " + ", ".join(sorted(map(fname, cycles))))
    if externs:
        print("extern calls charged %d B each: %s" % (EXTERN_COST, ", ".join(sorted(externs))))
    if unannotated:
        print("WARNING: pointer call sites not in INDIRECT (charged the fallback): " +
              ", ".join(sorted(unannotated)))
    unknown = [n for n in results if n != "main" and n not in PRIORITY]
    if unknown:
        print("WARNING: handlers with no known priority (not nested): " + ", ".join(unknown))


def find_elf(objdir):
    parent = os.path.dirname(objdir)
    kind = os.path.basename(objdir)  # "app" or "bl"
    elfs = glob.glob(os.path.join(parent, kind + "-*.elf"))
    if not elfs:
        sys.exit("no %s-*.elf next to %s" % (kind, objdir))
    return elfs[0]


def elf_functions(elf):
    out = subprocess.run([TOOL + "nm", elf], capture_output=True, text=True, check=True).stdout
    return {l.split()[2] for l in out.splitlines() if len(l.split()) == 3 and l.split()[1] in "TtWw"}


def address_taken(objdir, in_elf, live):
    """Functions whose address is stored somewhere: R_ARM_ABS32 relocations in
    code (literal pools) and data (vtables, tables), not in debug info."""
    titles_by_name = defaultdict(set)
    for t in live:
        titles_by_name[t.rsplit(":", 1)[-1]].add(t)
    taken = set()
    for obj in glob.glob(objdir + "/**/*.o", recursive=True):
        out = subprocess.run([TOOL + "objdump", "-r", obj], capture_output=True, text=True).stdout
        debug = False
        for l in out.splitlines():
            if l.startswith("RELOCATION RECORDS FOR"):
                debug = "[.debug" in l or "[.ARM.attributes" in l
                continue
            parts = l.split()
            if not debug and len(parts) >= 3 and parts[1] == "R_ARM_ABS32":
                sym = parts[2].split("+")[0]
                if sym.startswith(".text."):
                    sym = sym[len(".text."):]
                for t in titles_by_name.get(sym, ()):
                    taken.add(t)
    return taken


if __name__ == "__main__":
    main()
