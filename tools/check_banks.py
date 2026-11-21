#!/usr/bin/env python3
"""Fail the build if any ROM bank is over 16 KB (the linker lets code spill into the next bank without a word). Reads the .map."""
import re, sys
t = open(sys.argv[1]).read(); bad = 0
for m in re.finditer(r'^(_CODE(?:_(\d+))?|_HOME)\s+([0-9A-F]{8})\s+([0-9A-F]{8})', t, re.M):
    name, bank, a, l = m.group(1), m.group(2), int(m.group(3), 16), int(m.group(4), 16)
    end = (a & 0x3FFF) + l if bank else a + l
    # A segment starting partway into its bank has *less* than 16384 bytes
    # left. The previous test effectively checked only l > 16384, missing
    # smaller overflow across a bank boundary when a had a nonzero offset.
    free = 0x4000 - (a & 0x3FFF) - l if bank else 0x4000 - end
    if (bank and free < 0) or (not bank and end > 0x4000):
        print("BANK OVERFLOW: %s is %d bytes (ends at 0x%X)" % (name, l, a + l)); bad = 1
    elif l:
        print("%-8s %5d bytes" % (name, l) + (", %5d free" % free if bank or name == "_HOME" else " (bank 0, with _HOME)"))
        if bank == "19" and free < 384:
            print("WARNING: cockpit bank 19 has only %d bytes free; move new UI/AI into another bank" % free)
m = re.search(r'([0-9A-F]{8})\s+s__HEAP_E', t)
if m:
    end = int(m.group(1), 16); room = 0xD000 - end
    print("RAM      variables end at 0x%04X: %d bytes left for the stack (below D000)" % (end, room))
    # Keep one whole kilobyte between permanent data and the D000 stack
    # ceiling. The older 768-byte gate let deeply nested menus+VBlank run
    # with only 870 bytes on the real GBC ROM.
    if room < 0x400: print("RAM OVERFLOW: the stack needs at least 1024 bytes"); bad = 1
sys.exit(bad)
