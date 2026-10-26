#!/usr/bin/env python3
"""BUGSTAR GBC planet art (step 6).

Writes src/planet_art.c + src/planet_art.h and tools/planet_check.png (every ground tile in every kind of world).

The planet is a top-down map of 16x16 "metatiles" (four 8x8 tiles each). Each metatile is a 2-bit picture that uses
one of the four terrain palettes, BG palettes 4..7 (0..3 belong to the text). The palettes are a CHAIN, so that any two
neighbouring kinds of ground can be blended in one tile: deep water, shallow water, sand, grass.
  4 PA  deep, deep ripple, shallow, shallow light      (deep | shallow)
  5 PB  shallow, foam, sand, wet sand                   (shallow | sand)
  6 PC  sand, pebble, grass, grass shadow               (sand | grass)
  7 PF  grass, dark, light, flower                      (trees, bushes, flowers, grass itself)
Each tile that sits on the edge between two kinds is one of 42 BLEND tiles: the top-left quarter of the cell, worked out
from the cell and its three neighbours on that side (left, up, diagonal); the other three quarters are flips of those.
That gives rounded shores, a foam line, a shadow along the grass: no square patches.
Every kind of world (GARDEN, DESERT, ICE, JUNGLE, OCEAN, DUSK) has its own colours for those four palettes, so the
same tiles become a meadow, a dune field, a snowfield... The tiles live in VRAM bank 1 (the font fills bank 0).
The ship you land in is a sprite: 32x24, cut into four 8x16 columns (two rows), one sprite palette.

Run:  python3 tools/make_planet.py
"""
import os, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = random.Random(7)

MT = ["DEEP", "SHALLOW", "SAND", "GRASS", "GRASS2", "FLOWERS", "TREE", "ROCK", "BERRY", "DOOR", "PAD", "HOUSE", "STALL", "WALL", "GATE"]
PAL = {"DEEP": 0, "SHALLOW": 0, "SAND": 2, "GRASS": 3, "GRASS2": 3, "FLOWERS": 3, "TREE": 3, "ROCK": 2, "BERRY": 3, "DOOR": 2, "PAD": 2, "HOUSE": 3, "STALL": 3, "WALL": 2, "GATE": 2}
LVL = {"DEEP": 0, "SHALLOW": 1, "SAND": 2, "GRASS": 3, "GRASS2": 3, "FLOWERS": 3, "TREE": 3, "ROCK": 2, "BERRY": 3, "DOOR": 2, "PAD": 2, "HOUSE": 3, "STALL": 3, "WALL": 2, "GATE": 2}
OBJ = ("TREE", "ROCK", "BERRY", "DOOR", "PAD", "HOUSE", "STALL", "WALL", "GATE")      # drawn as they are, never blended


def grid(v=0):
    return [[v] * 16 for _ in range(16)]


def disc(g, cx, cy, r, v, sq=1.0):
    for y in range(16):
        for x in range(16):
            if ((x - cx) ** 2 + ((y - cy) * sq) ** 2) <= r * r:
                g[y][x] = v


def art(name):
    g = grid()
    if name == "DEEP":                                        # PA: 0 deep, 1 ripple
        g = grid(0)
        for _ in range(7):
            x, y = R.randrange(13), R.randrange(16)
            for k in range(R.randrange(2, 4)): g[y][x + k] = 1
    elif name == "SHALLOW":                                   # PA: 2 shallow, 3 light
        g = grid(2)
        for _ in range(7):
            x, y = R.randrange(12), R.randrange(16)
            for k in range(R.randrange(2, 5)): g[y][x + k] = 3
    elif name == "SAND":                                      # PC: 0 sand, 1 pebble
        g = grid(0)
        for _ in range(9): g[R.randrange(16)][R.randrange(16)] = 1
    elif name in ("GRASS", "GRASS2"):                         # PF: 0 grass, 1 dark, 2 light
        g = grid(0)
        n = 9 if name == "GRASS" else 14
        for _ in range(n):
            x, y = R.randrange(16), R.randrange(1, 16)
            g[y][x] = 1; g[y - 1][x] = 2                     # a blade: dark root, lit tip
        if name == "GRASS2":
            for _ in range(5): g[R.randrange(16)][R.randrange(16)] = 2
    elif name == "FLOWERS":
        g = art("GRASS")
        for (x, y) in ((3, 4), (11, 3), (6, 11), (13, 12)):
            for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)): g[y + dy][x + dx] = 3
            g[y][x] = 2
    elif name == "TREE":                                      # PF: a round crown with a dark rim, lit top-left, the trunk below
        g = grid(0)
        disc(g, 7.5, 7.0, 7.4, 1)
        disc(g, 7.5, 7.0, 6.2, 0)
        disc(g, 6.0, 5.2, 4.0, 2)
        disc(g, 5.6, 4.8, 2.0, 0)
        for x, y in ((9, 4), (11, 7), (4, 9), (8, 10), (10, 9)): g[y][x] = 1
        for y in range(13, 16):
            for x in range(6, 10): g[y][x] = 1
    elif name == "ROCK":                                      # PC: 0 sand, 1 stone, 3 shadow
        g = art("SAND")
        disc(g, 7.5, 9.5, 6.8, 3, 1.3)
        disc(g, 7.5, 8.8, 5.6, 1, 1.3)
        disc(g, 5.8, 6.8, 2.3, 0)
        for x, y in ((9, 10), (10, 9), (6, 11), (11, 11)): g[y][x] = 3
    elif name == "BERRY":                                     # PF: a bush, light, rim dark, berries
        g = art("GRASS")
        disc(g, 7.5, 8.5, 6.6, 1)
        disc(g, 7.5, 8.0, 5.6, 2)
        disc(g, 5.8, 6.0, 2.0, 0)
        for x, y in ((5, 9), (9, 7), (10, 11), (7, 12), (4, 6)):
            g[y][x] = 3; g[y][x + 1] = 3
    elif name == "DOOR":                                      # PC: a stone slab with stairs going down
        g = grid(1)
        for y in range(16):
            for x in range(16): g[y][x] = 1 if (x in (0, 15) or y in (0, 15)) else 3
        for k, y in enumerate((3, 6, 9, 12)):
            for x in range(3 + k, 13 - k): g[y][x] = 1; g[y + 1][x] = 0
    elif name == "PAD":                                       # PC: sand with grey joints, like paving laid on the ground
        g = art("SAND")
        for i in range(16):
            if i % 5 != 4: g[0][i] = 1; g[i][0] = 1
    elif name == "HOUSE":                                      # PF: 0 grass, 1 dark outline, 2 light wall, 3 roof (the flower colour of the world)
        g = grid(0)
        for y in range(0, 8):
            hw = min(8, 5 + y)
            for x in range(8 - hw, 8 + hw):
                g[y][x] = 1 if (x == 8 - hw or x == 7 + hw or y == 0 or y == 7) else (3 if y % 3 else 1)
        for y in range(8, 16):
            for x in range(1, 15): g[y][x] = 1 if (x in (1, 14) or y == 15) else 2
        for y in range(10, 15):
            for x in range(6, 10): g[y][x] = 1
        for y in (10, 11):
            for x in (3, 4, 11, 12): g[y][x] = 1
    elif name == "STALL":                                     # a market stall: striped awning, a counter with goods
        g = grid(0)
        for y in range(0, 7):
            for x in range(0, 16): g[y][x] = 1 if (y == 0 or y == 6) else (3 if (x // 2) % 2 == 0 else 2)
        for y in range(7, 16):
            g[y][1] = g[y][14] = 1
        for y in range(10, 16):
            for x in range(2, 14): g[y][x] = 1 if y == 15 else 2
        for x in range(2, 14): g[10][x] = 1
        for x in (4, 7, 10): g[9][x] = 3; g[9][x + 1] = 2
    elif name in ("WALL", "GATE"):                            # PC: 0 sand, 1 pebble grey, 2 grass, 3 grass shadow: a sandstone wall, grey mortar
        g = grid(0)
        for x in range(16):
            if (x // 4) % 2 == 0: g[0][x] = g[1][x] = 2          # battlement gaps show the lawn behind
            g[2][x] = 1; g[3][x] = 3 if (x // 4) % 2 == 0 else 1
        for y in range(4, 16):
            for x in range(16):
                row = (y - 4) // 4
                if (y - 4) % 4 == 3 or (x + (4 if row % 2 else 0)) % 8 == 7: g[y][x] = 1
                elif R.random() < 0.06: g[y][x] = 1
        if name == "GATE":
            for y in range(5, 16):
                for x in range(4, 12):
                    d = abs(x - 7.5)
                    if y > 8 or d <= 3 - (8 - y) * 0.8: g[y][x] = 3 if y < 15 else 0
            for y in range(4, 8):
                for x in range(3, 13): g[y][x] = 1 if (g[y][x] == 0 and (x + y) % 2 == 0) else g[y][x]
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



# small things that live on the planet (sprites, 8x16 cells with the picture in the top 8 rows):
# 0,1 a twinkling treasure spot (2 frames), 2,3 a butterfly (wings open, wings shut). palette 1 gold / white, palette 2 pink wings.
CRIT_PAL = [(0, 0, 0), (20, 12, 1), (31, 22, 6), (31, 31, 26), (0, 0, 0), (12, 3, 9), (31, 14, 22), (31, 29, 30)]
SPARK = [
    ["...33...", "...33...", "..3223..", "33222233", "33222233", "..3223..", "...33...", "...33..."],
    ["........", "...33...", "...33...", "..3223..", "..3223..", "...33...", "...33...", "........"],
]
def butterfly(open_):
    g = [[0] * 8 for _ in range(8)]
    rx, ry, cx = (2.6, 2.7, 1.9) if open_ else (1.5, 2.6, 2.6)
    for y in range(8):
        for x in range(4):
            if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - 3.4) / ry) ** 2 <= 1.0:
                g[y][x] = 2; g[y][7 - x] = 2
    for y in range(8):
        for x in range(8):
            if g[y][x] == 2 and (x == 0 or y == 0 or g[y][x - 1] == 0 and x < 4 or (x > 3 and (x == 7 or g[y][x + 1] == 0)) or y == 0 or g[y - 1][x] == 0 or (y < 7 and g[y + 1][x] == 0)):
                g[y][x] = 1
    for y in range(1, 3):
        if g[y][1 if open_ else 2] == 2: g[y][1 if open_ else 2] = 3
    for y in range(2, 7): g[y][3] = g[y][4] = 1
    g[1][2] = g[1][5] = 1                                      # feelers
    return g
def crit_tiles():
    out = []
    for f in (SPARK[0], SPARK[1]):
        g = [[".123".index(ch) for ch in r] for r in f]
        out += tile2bpp(g, 0, 0) + [0] * 16
    for o in (True, False):
        g = butterfly(o)
        out += tile2bpp(g, 0, 0) + [0] * 16
    return out


BLEND_C, BLEND_P = 40.0, 0.8           # how soft the corners are (inverse-distance weighting between the four cell centres)


def hn(a, b, c):
    return ((a * 73856093) ^ (b * 19349663) ^ (c * 83492791)) & 0xFFFF


def blend_quarter(m, pat):
    """The top-left quarter of a cell: pat bits 0..3 = is the cell / its left / its upper / its diagonal neighbour the
    higher kind (m + 1). Returns an 8x8 grid of palette indices: 0 low, 1 low detail, 2 high, 3 high detail."""
    his = [(pat >> b) & 1 for b in range(4)]
    cs = [(8, 8), (-8, 8), (8, -8), (-8, -8)]
    g = [[0] * 8 for _ in range(8)]
    for y in range(8):
        for x in range(8):
            X, Y = x + 0.5, y + 0.5
            num = den = 0.0
            for h, (cx, cy) in zip(his, cs):
                w = 1.0 / (((X - cx) ** 2 + (Y - cy) ** 2) + BLEND_C) ** BLEND_P
                num += w * h; den += w
            v = num / den
            j = ((hn(x, y, m * 16 + pat) & 255) / 255 - .5) * 0.12
            if v + j > 0.5:
                g[y][x] = 3 if v + j < 0.60 else 2
            else:
                if v + j > 0.40 and (m == 1 or (hn(x, y, 77 + pat) & 3) == 0): g[y][x] = 1
                elif (hn(x, y, 99 + pat) & 255) < 12: g[y][x] = 1 if m != 1 else 0
                else: g[y][x] = 0
    return g


def world_pals(w):
    def mix(a, b): return tuple((x + y) // 2 for x, y in zip(a, b))
    wa, sa, ga, _t = w[1:]
    return ([wa[0], mix(wa[0], wa[1]), wa[1], wa[2]],      # PA deep | shallow
            [wa[1], wa[2], sa[1], sa[0]],                  # PB shallow | sand
            [sa[1], sa[3], ga[1], ga[0]],                  # PC sand | grass
            [ga[1], ga[0], ga[2], ga[3]])                  # PF grass things


def main():
    tiles = []
    arts = {}
    for n in MT:
        g = art(n); arts[n] = g
        for (x0, y0) in ((0, 0), (8, 0), (0, 8), (8, 8)): tiles += tile2bpp(g, x0, y0)
    nblend = 0
    for m in range(3):
        for pat in range(1, 15):
            q = blend_quarter(m, pat)
            gg = [[q[y][x] if x < 8 and y < 8 else 0 for x in range(16)] for y in range(16)]
            tiles += tile2bpp(gg, 0, 0); nblend += 1
    sg = [[".123".index(c) for c in r] for r in SHIP]
    stiles = []
    for col in range(4):                                       # 8x16 sprites: column by column, top row then bottom row
        for row in range(2):
            for half in range(2): stiles += tile2bpp(sg, col * 8, row * 16 + half * 8)
    ctl = crit_tiles()
    c = ["#pragma bank 4", "/* generated by tools/make_planet.py: do not edit */", "#include <gb/gb.h>", "#include <gb/cgb.h>", '#include "planet_art.h"']
    c.append("const UINT8 planet_tiles[PL_NTILES * 16] = {" + ",".join("0x%02X" % b for b in tiles) + "};")
    c.append("const UINT8 planet_mtpal[PL_NMT] = {" + ",".join(str(PAL[n]) for n in MT) + "};")
    c.append("const UINT8 planet_mtlvl[PL_NMT] = {" + ",".join(str(LVL[n] + (4 if n in OBJ else 0)) for n in MT) + "};")
    pals = []
    for w in WORLDS:
        for p in world_pals(w): pals += ["RGB(%d,%d,%d)" % col for col in p]
    c.append("const palette_color_t planet_pals[PL_NWORLD * 16] = {" + ",".join(pals) + "};")
    c.append('const char *const planet_world_name[PL_NWORLD] = {' + ",".join('"%s"' % w[0] for w in WORLDS) + "};")
    c.append("const UINT8 ship_tiles[16 * 16] = {" + ",".join("0x%02X" % b for b in stiles) + "};")
    perm = list(range(256)); random.Random(1999).shuffle(perm)       # the noise's shuffle table, and its smoothing curve
    c.append("const UINT8 planet_perm[256] = {" + ",".join(str(v) for v in perm) + "};")
    ease = [min(255, int(round(255 * ((f / 256) ** 2 * (3 - 2 * f / 256))))) for f in range(256)]
    c.append("const UINT8 planet_ease[256] = {" + ",".join(str(v) for v in ease) + "};")
    c.append("const UINT8 crit_tiles[8 * 16] = {" + ",".join("0x%02X" % b for b in ctl) + "};")
    c.append("const palette_color_t crit_pal[8] = {" + ",".join("RGB(%d,%d,%d)" % col for col in CRIT_PAL) + "};")
    c.append("const palette_color_t ship_pal[4] = {" + ",".join("RGB(%d,%d,%d)" % col for col in SHIP_PAL) + "};")
    open(os.path.join(ROOT, "src", "planet_art.c"), "w").write("\n".join(c) + "\n")
    h = ["#ifndef PLANET_ART_H", "#define PLANET_ART_H", "#include <gb/gb.h>", "#include <gb/cgb.h>",
         "/* generated by tools/make_planet.py: the planet's ground tiles (bank 4) */",
         "enum { " + ", ".join("MT_" + n for n in MT) + ", PL_NMT };",
         "#define PL_NWORLD %d" % len(WORLDS),
         "#define PL_NTILES (PL_NMT * 4 + 42)       /* the metatile pictures, then the blend tiles (index PL_NMT * 4 + m * 14 + pat - 1) */",
         "enum { " + ", ".join("W_" + w[0] for w in WORLDS) + " };",
         "extern const UINT8 planet_tiles[PL_NTILES * 16];      /* four 8x8 tiles per metatile: top-left, top-right, bottom-left, bottom-right */",
         "extern const UINT8 planet_mtpal[PL_NMT];           /* which terrain palette (0..3 = BG palette 4..7) */",
         "extern const UINT8 planet_mtlvl[PL_NMT];           /* kind of ground: 0 deep, 1 shallow, 2 sand, 3 grass (+4: an object that is drawn as it is) */",
         "extern const palette_color_t planet_pals[PL_NWORLD * 16];",
         "extern const char *const planet_world_name[PL_NWORLD];",
         "extern const UINT8 ship_tiles[16 * 16];            /* 8 sprites of 8x16, column by column */",
         "extern const palette_color_t ship_pal[4];",
         "extern const UINT8 crit_tiles[8 * 16];             /* sparkle x2, butterfly x2: 8x16 sprites, the picture in the top tile */",
         "extern const palette_color_t crit_pal[8];          /* sprite palette 1: gold and white (sparkles, the golden moth); 2: pink (butterflies) */",
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
                pal = world_pals(w)[PAL[n]]
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
