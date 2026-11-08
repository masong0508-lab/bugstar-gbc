#!/usr/bin/env python3
"""Writes make dependencies (object -> the headers its .c includes, followed through), so changing one header rebuilds only the files that use it.
usage: deps.py BUILD_DIR > deps.mk"""
import os, re, sys
src = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src")
inc = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
cache = {}
def deps(f, seen):
    if f in seen: return
    seen.add(f)
    p = os.path.join(src, f)
    if not os.path.exists(p): return
    if f not in cache: cache[f] = inc.findall(open(p, errors="replace").read())
    for h in cache[f]: deps(h, seen)
b = sys.argv[1]
for f in sorted(os.listdir(src)):
    if f.endswith(".c"):
        seen = set(); deps(f, seen); seen.discard(f)
        print("%s/%s.o: %s" % (b, f[:-2], " ".join("src/" + h for h in sorted(seen))))
