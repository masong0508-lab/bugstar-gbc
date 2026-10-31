#!/usr/bin/env python3
"""RAM report from the linker .map (never fails the build). Shows where the always-there RAM (C000-CFFF) goes and how much room the stack has.
Usage: python3 tools/ram_report.py bugstar.map"""
import re, sys
try:
    t = open(sys.argv[1]).read()
    syms = {}
    for m in re.finditer(r'^\s+([0-9A-F]{8})\s+(\S+)\s*(\S*)\s*$', t, re.M):
        a = int(m.group(1), 16); n = m.group(2)
        if 0xC000 <= a < 0xD000 and not n.startswith(('s__', 'l__', '.')) and n not in ('__start', '__end'):
            syms.setdefault(a, (n, m.group(3)))
    end = None
    m = re.search(r'([0-9A-F]{8})\s+s__HEAP_E', t)
    if m: end = int(m.group(1), 16)
    addrs = sorted(syms)
    rows = []
    for i, a in enumerate(addrs):
        nxt = addrs[i + 1] if i + 1 < len(addrs) else (end if end and end > a else a)
        rows.append((nxt - a, syms[a][0], syms[a][1]))
    rows.sort(reverse=True)
    mods = {}
    for sz, n, mod in rows: mods[mod or '?'] = mods.get(mod or '?', 0) + sz
    print("RAM report (always-there RAM, C000-CFFF; sizes are gaps between symbols, so +-a few bytes):")
    for sz, n, mod in rows[:20]: print("  %5d  %-22s %s" % (sz, n, mod))
    print("  by file:", ", ".join("%s %d" % (k, v) for k, v in sorted(mods.items(), key=lambda kv: -kv[1])[:10]))
    if end:
        room = 0xD000 - end
        print("  stack room: %d bytes (variables end at 0x%04X)" % (room, end))
        if room < 1024: print("  WARNING: under 1 KB of stack. Deep menus + dialogue + the VBL interrupt can run into the variables; free RAM before adding features.")
except Exception as e:
    print("ram_report: skipped (%s)" % e)
