#!/usr/bin/env python3
"""Fail the build if any ROM bank is over 16 KB (the linker lets code spill into the next bank without a word). Reads the .map."""
import re, sys
t = open(sys.argv[1]).read(); bad = 0
for m in re.finditer(r'^(_CODE(?:_(\d+))?|_HOME)\s+([0-9A-F]{8})\s+([0-9A-F]{8})', t, re.M):
    name, bank, a, l = m.group(1), m.group(2), int(m.group(3), 16), int(m.group(4), 16)
    end = (a & 0x3FFF) + l if bank else a + l
    if (bank and end > 0x4000 + (a & 0x3FFF) and l > 0x4000 - (a & 0x3FFF)) or (not bank and end > 0x4000):
        print("BANK OVERFLOW: %s is %d bytes (ends at 0x%X)" % (name, l, a + l)); bad = 1
    elif l: print("%-8s %5d bytes, %5d free" % (name, l, (0x4000 - (a & 0x3FFF) - l) if bank else 0x4000 - end))
sys.exit(bad)
