#!/usr/bin/env python3
"""BUGSTAR GBC planet art (step 6).

Writes src/planet_art.c + src/planet_art.h and tools/planet_check.png (every ground tile in every kind of world).

The planet is a top-down map of 16x16 "metatiles" (four 8x8 tiles each). Each metatile is a 2-bit picture that uses
one of the four terrain palettes, BG palettes 4..7 (0..3 belong to the text):
  4 WATER  deep, mid, light, foam
  5 GROUND sand dark, sand, sand light, stone
  6 GRASS  dark, mid, light, flower
  7 TREE   shadow, leaves, leaves lit, trunk
Every kind of world (GARDEN, DESERT, ICE, JUNGLE, OCEAN, DUSK) has its own colours for those four palettes, so the
same tiles become a meadow, a dune field, a snowfield... The tiles live in VRAM bank 1 (the font fills bank 0).
The ship you land in is a sprite: 32x24, cut into four 8x16 columns (two rows), one sprite palette.

Run:  python3 tools/make_planet.py
"""
import os, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = random.Random(7)

MT = ["DEEP", "SHALLOW", "SAND", "GRASS", "GRASS2", "FLOWERS", "TREE", "ROCK", "BERRY", "DOOR", "PAD"]
PAL = {"DEEP": 0, "SHALLOW": 0, "SAND": 1, "GRASS": 2, "GRASS2": 2, "FLOWERS": 2, "TREE": 3, "ROCK": 1, "BERRY": 2, "DOOR": 1, "PAD": 1}


def grid(v=0):
    return [[v] * 16 for _ in range(16)]


def disc(g, cx, cy, r, v, sq=1.0):
    for y in range(16):
        for x in range(16):
            if ((x - cx) ** 2 + ((y - cy) * sq) ** 2) <= r * r:
                g[y][x] = v


def art(name):
    g = grid()
    if name == "DEEP":
        g = grid(0)
        for _ in range(5):
            x, y = R.randrange(14), R.randrange(16)
            g[y][x] = g[y][x + 1] = 1
        for _ in range(2):
            x, y = R.randrange(15), R.randrange(16); g[y][x] = 2
    elif name == "SHALLOW":
        g = grid(1)
        for y in (3, 10):
            for x in range(16):
                if (x + y) % 6 < 3: g[y + (x // 3) % 2][x] = 2
        for _ in range(4):
            g[R.randrange(16)][R.randrange(16)] = 3
    elif name == "SAND":
        g = grid(1)
        for _ in range(10): g[R.randrange(16)][R.randrange(16)] = 2
        for _ in range(6): g[R.randrange(16)][R.randrange(16)] = 0
    elif name in ("GRASS", "GRASS2"):
        g = grid(1)
        n = 9 if name == "GRASS" else 14
        for _ in range(n):
            x, y = R.randrange(16), R.randrange(1, 16)
            g[y][x] = 0; g[y - 1][x] = 2                     # a blade: dark root, lit tip
        if name == "GRASS2":
            for _ in range(6): g[R.randrange(16)][R.randrange(16)] = 2
    elif name == "FLOWERS":
        g = art("GRASS")
        for (x, y) in ((3, 4), (11, 3), (6, 11), (13, 12)):
            for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)): g[y + dy][x + dx] = 3
            g[y][x] = 2
    elif name == "TREE":
        g = grid(0)                                           # a round crown that fills the tile; the trunk peeks out below
        disc(g, 7.5, 6.5, 7.2, 1)
        disc(g, 5.5, 4.5, 3.6, 2)
        for x, y in ((9, 3), (11, 6), (4, 9), (8, 9)): g[y][x] = 2
        for x, y in ((10, 10), (6, 11), (12, 8)): g[y][x] = 0
        for y in range(12, 16):
            for x in range(6, 10): g[y][x] = 3 if (x != 9) else 0
        for x in range(4, 12): g[15][x] = 0
    elif name == "ROCK":
        g = art("SAND")
        disc(g, 7.5, 9, 6.5, 0, 1.25)
        disc(g, 7.5, 8.5, 5.5, 3, 1.25)
        disc(g, 6, 7, 2.2, 2)
    elif name == "BERRY":
        g = art("GRASS")
        disc(g, 7.5, 8.5, 6.5, 0)
        disc(g, 7.5, 8.0, 5.5, 1)
        disc(g, 6, 6, 2.5, 2)
        for x, y in ((5, 9), (9, 7), (10, 11), (7, 12), (4, 6)): g[y][x] = 3
    elif name == "DOOR":
        g = art("SAND")
        for y in range(2, 15):
            for x in range(2, 14): g[y][x] = 3 if (x in (2, 13) or y == 2) else 0   # a stone frame, dark inside
        for k, y in enumerate((5, 8, 11)):
            for x in range(4 + k, 12 - k): g[y][x] = 2                              # steps going down
    elif name == "PAD":
        g = art("SAND")
        for x in range(16): g[0][x] = 2 if x % 4 < 2 else 1
    return g


# the kinds of world: four palettes of four colours each (5-bit RGB), and what the ground does
WORLDS = [
    ("GARDEN", [(2, 6, 16), (5, 13, 25), (12, 22, 30), (28, 30, 31)], [(18, 14, 8), (26, 22, 13), (30, 28, 20), (14, 14, 16)],
               [(4, 12, 4), (9, 20, 6), (16, 26, 9), (30, 8, 12)], [(2, 8, 4), (6, 16, 6), (12, 24, 10), (14, 9, 4)]),
    ("DESERT", [(3, 10, 16), (8, 18, 24), (16, 26, 30), (30, 31, 31)], [(20, 12, 5), (29, 21, 10), (31, 27, 17), (16, 12, 10)],
               [(14, 12, 4), (22, 19, 8), (28, 25, 13), (30, 14, 4)], [(4, 12, 4), (8, 20, 6), (14, 27, 10), (24, 18, 9)]),
    ("ICE",    [(2, 6, 14), (6, 14, 24), (16, 24, 31), (31, 31, 31)], [(14, 18, 24), (22, 26, 30), (29, 31, 31), (12, 14, 20)],
               [(18, 22, 28), (25, 28, 31), (31, 31, 31), (10, 20, 30)], [(2, 8, 8), (4, 14, 10), (20, 26, 28), (10, 7, 4)]),
    ("JUNGLE", [(2, 8, 8), (4, 16, 14), (10, 24, 20), (24, 30, 26)], [(14, 10, 4), (20, 16, 8), (26, 22, 13), (10, 12, 10)],
               [(2, 10, 2), (5, 18, 4), (11, 25, 6), (31, 16, 2)], [(1, 6, 2), (3, 13, 3), (8, 21, 6), (12, 7, 2)]),
    ("OCEAN",  [(1, 4, 14), (3, 10, 22), (10, 20, 30), (28, 31, 31)], [(24, 22, 15), (30, 28, 20), (31, 31, 26), (16, 16, 18)],
               [(6, 16, 10), (11, 24, 13), (19, 29, 18), (31, 24, 6)], [(3, 11, 6), (7, 19, 9), (14, 27, 13), (16, 10, 5)]),
    ("DUSK",   [(6, 2, 10), (12, 4, 16), (20, 10, 24), (30, 22, 30)], [(10, 8, 10), (15, 12, 14), (21, 18, 20), (8, 6, 10)],
               [(6, 4, 10), (11, 8, 16), (17, 13, 22), (26, 6, 10)], [(3, 2, 6), (8, 5, 12), (14, 10, 18), (10, 6, 6)]),
]

# the ship: 32x24, 0 = clear, 1 dark, 2 hull, 3 light
SHIP = [
    "..............11................",
    ".............1331...............",
    "............133331..............",
    "...........12233221.............",
    "...........12333321.............",
    "..........1223333221............",
    "..11......1222222221......11....",
    ".1321....122233322221....1231...",
    ".13221..12222333222221..12231...",
    ".122221122222222222222112222....",
    ".1222222222222222222222222221...",
    "..122222222222222222222222221...",
    "...1222222111111111122222221....",
    "....12221.............12221.....",
    ".....121...............121......",
    "......1.................1.......",
]
SHIP = [r.ljust(32, ".") for r in SHIP] + ["." * 32] * (32 - len(SHIP))
SHIP_PAL = [(0, 0, 0), (6, 4, 8), (26, 6, 8), (31, 24, 20)]


def tile2bpp(g, x0, y0):
    out = []
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            v = g[y0 + y][x0 + x]
            lo |= (v & 1) << (7 - x); hi |= ((v >> 1) & 1) << (7 - x)
        out += [lo, hi]
    return out


def main():
    tiles = []
    arts = {}
    for n in MT:
        g = art(n); arts[n] = g
        for (x0, y0) in ((0, 0), (8, 0), (0, 8), (8, 8)): tiles += tile2bpp(g, x0, y0)
    sg = [[".123".index(c) for c in r] for r in SHIP]
    stiles = []
    for col in range(4):                                       # 8x16 sprites: column by column, top row then bottom row
        for row in range(2):
            for half in range(2): stiles += tile2bpp(sg, col * 8, row * 16 + half * 8)
    c = ["#pragma bank 4", "/* generated by tools/make_planet.py: do not edit */", "#include <gb/gb.h>", "#include <gb/cgb.h>", '#include "planet_art.h"']
    c.append("const UINT8 planet_tiles[PL_NMT * 64] = {" + ",".join("0x%02X" % b for b in tiles) + "};")
    c.append("const UINT8 planet_mtpal[PL_NMT] = {" + ",".join(str(PAL[n]) for n in MT) + "};")
    pals = []
    for w in WORLDS:
        for p in w[1:]:
            pals += ["RGB(%d,%d,%d)" % col for col in p]
    c.append("const palette_color_t planet_pals[PL_NWORLD * 16] = {" + ",".join(pals) + "};")
    c.append('const char *const planet_world_name[PL_NWORLD] = {' + ",".join('"%s"' % w[0] for w in WORLDS) + "};")
    c.append("const UINT8 ship_tiles[16 * 16] = {" + ",".join("0x%02X" % b for b in stiles) + "};")
    perm = list(range(256)); random.Random(1999).shuffle(perm)       # the noise's shuffle table, and its smoothing curve
    c.append("const UINT8 planet_perm[256] = {" + ",".join(str(v) for v in perm) + "};")
    ease = [min(255, int(round(255 * ((f / 256) ** 2 * (3 - 2 * f / 256))))) for f in range(256)]
    c.append("const UINT8 planet_ease[256] = {" + ",".join(str(v) for v in ease) + "};")
    c.append("const palette_color_t ship_pal[4] = {" + ",".join("RGB(%d,%d,%d)" % col for col in SHIP_PAL) + "};")
    open(os.path.join(ROOT, "src", "planet_art.c"), "w").write("\n".join(c) + "\n")
    h = ["#ifndef PLANET_ART_H", "#define PLANET_ART_H", "#include <gb/gb.h>", "#include <gb/cgb.h>",
         "/* generated by tools/make_planet.py: the planet's ground tiles (bank 4) */",
         "enum { " + ", ".join("MT_" + n for n in MT) + ", PL_NMT };",
         "#define PL_NWORLD %d" % len(WORLDS),
         "enum { " + ", ".join("W_" + w[0] for w in WORLDS) + " };",
         "extern const UINT8 planet_tiles[PL_NMT * 64];      /* four 8x8 tiles per metatile: top-left, top-right, bottom-left, bottom-right */",
         "extern const UINT8 planet_mtpal[PL_NMT];           /* which terrain palette (0..3 = BG palette 4..7) */",
         "extern const palette_color_t planet_pals[PL_NWORLD * 16];",
         "extern const char *const planet_world_name[PL_NWORLD];",
         "extern const UINT8 ship_tiles[16 * 16];            /* 8 sprites of 8x16, column by column */",
         "extern const palette_color_t ship_pal[4];",
         "extern const UINT8 planet_perm[256];               /* a shuffle of 0..255: the noise hashes with it, no multiplying */",
         "extern const UINT8 planet_ease[256];               /* 3t^2 - 2t^3, so hills are round */",
         "#endif"]
    open(os.path.join(ROOT, "src", "planet_art.h"), "w").write("\n".join(h) + "\n")
    try:
        from PIL import Image
        S = 3
        im = Image.new("RGB", ((len(MT) * 18) * S, (len(WORLDS) * 18 + 30) * S), (40, 40, 40))
        for wi, w in enumerate(WORLDS):
            for mi, n in enumerate(MT):
                pal = w[1 + PAL[n]]
                for y in range(16):
                    for x in range(16):
                        r, g, b = pal[arts[n][y][x]]
                        for yy in range(S):
                            for xx in range(S): im.putpixel(((mi * 18 + x) * S + xx, (wi * 18 + y) * S + yy), (r * 8, g * 8, b * 8))
        for y in range(16):
            for x in range(32):
                v = sg[y][x]
                if v:
                    r, g, b = SHIP_PAL[v]
                    for yy in range(S):
                        for xx in range(S): im.putpixel(((x) * S + xx, (len(WORLDS) * 18 + 4 + y) * S + yy), (r * 8, g * 8, b * 8))
        im.save(os.path.join(ROOT, "tools", "planet_check.png"))
    except ImportError:
        pass
    print("planet art: %d metatiles, %d worlds" % (len(MT), len(WORLDS)))


if __name__ == "__main__":
    main()
