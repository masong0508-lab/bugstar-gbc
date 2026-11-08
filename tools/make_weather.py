#!/usr/bin/env python3
"""BUGSTAR GBC weather art (version 6): the falling things of src/weather.c. Writes src/weather_art.c (bank 44).
Each particle is one 8x16 sprite (top tile + bottom tile, 2 bits a pixel), drawn with sprite palette 1 (0 clear, 1 dark gold, 2 gold, 3 pale)."""
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def blank(): return [[0] * 8 for _ in range(16)]
def put(g, pts, v):
    for (x, y) in pts: g[y][x] = v
def tiles(g):
    out = []
    for r0 in (0, 8):
        for y in range(r0, r0 + 8):
            lo = hi = 0
            for x in range(8):
                v = g[y][x]; lo |= (v & 1) << (7 - x); hi |= (v >> 1) << (7 - x)
            out += [lo, hi]
    return out

P = []
g = blank()                                   # 0 rain: a long thin streak leaning left
for i in range(14): g[i + 1][6 - i // 3] = 3 if i % 5 else 2
P.append(g)
g = blank()                                   # 1 snow: a soft flake
put(g, [(3, 6), (3, 8), (2, 7), (4, 7)], 3); put(g, [(3, 7)], 3); put(g, [(1, 7), (5, 7), (3, 5), (3, 9)], 1)
P.append(g)
g = blank()                                   # 2 ember: a bright spark with a dark halo
put(g, [(3, 7), (4, 7), (3, 8), (4, 8)], 2); put(g, [(3, 7)], 3); put(g, [(2, 7), (5, 8), (4, 6), (3, 9)], 1)
P.append(g)
g = blank()                                   # 3 spore: a round glow
put(g, [(3, 7), (4, 7), (3, 8), (4, 8)], 3); put(g, [(2, 7), (5, 7), (2, 8), (5, 8), (3, 6), (4, 6), (3, 9), (4, 9)], 1)
P.append(g)
g = blank()                                   # 4 dust: a long horizontal streak
put(g, [(x, 7) for x in range(0, 8)], 1); put(g, [(x, 8) for x in range(1, 7)], 2); put(g, [(x, 8) for x in range(3, 6)], 3)
P.append(g)
g = blank()                                   # 5 shard: a four-pointed sparkle
put(g, [(4, 5), (4, 6), (4, 7), (4, 8), (4, 9), (2, 7), (3, 7), (5, 7), (6, 7)], 2); put(g, [(4, 7)], 3); put(g, [(3, 6), (5, 6), (3, 8), (5, 8)], 1)
P.append(g)
g = blank()                                   # 6 confetti: a little tumbling ribbon
put(g, [(3, 6), (4, 6), (3, 7), (4, 7), (3, 8)], 2); put(g, [(4, 8), (4, 9)], 3); put(g, [(2, 7), (5, 6)], 1)
P.append(g)
g = blank()                                   # 7 confetti, turned over
put(g, [(2, 7), (3, 7), (4, 7), (5, 7), (2, 8), (3, 8)], 3); put(g, [(4, 8), (5, 8)], 2); put(g, [(3, 6), (4, 9)], 1)
P.append(g)

c = ["#pragma bank 44", "/* made by tools/make_weather.py: do not edit */", "#include <gb/gb.h>",
     "const UINT8 wx_tiles[%d] = {%s};" % (len(P) * 32, ",".join("%d" % b for g in P for b in tiles(g)))]
open(os.path.join(ROOT, "src", "weather_art.c"), "w").write("\n".join(c) + "\n")
try:
    from PIL import Image
    im = Image.new("RGB", (len(P) * 10, 18), (40, 60, 40)); pal = [(40, 60, 40), (150, 90, 8), (248, 176, 48), (248, 248, 208)]
    for i, g in enumerate(P):
        for y in range(16):
            for x in range(8): im.putpixel((i * 10 + x, y), pal[g[y][x]])
    im.resize((im.width * 6, im.height * 6), Image.NEAREST).save(os.path.join(ROOT, "tools", "weather_check.png"))
except ImportError: pass
print("weather art: %d particles" % len(P))
