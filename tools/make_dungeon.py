#!/usr/bin/env python3
"""BUGSTAR GBC dungeon art (step 7).

Writes src/dungeon_art.c + src/dungeon_art.h and tools/dungeon_check.png.

Ground: 16x16 metatiles (four 8x8 tiles, VRAM bank 1), each in one of the BG palettes 4..7:
  4 STONE  wall shadow, wall, wall lit, mortar        5 FLOOR  dark, floor, floor lit, crack
  6 GOLD   dark, wood, gold, shine (chests, stairs)   7 CAGE   dark, bars, lit, the friend inside
Monsters: 16x16 sprites (two 8x16 sprites side by side, two frames), the queen 32x32 (eight 8x16 sprites, two frames).
Sprite palettes 0..3 (the bug has 4..7): 0 spider/queen, 1 beetle, 2 wasp, 3 slime and the bite flash.
Each kind of dungeon (the same six kinds as the worlds) recolours the stone and floor.

Run:  python3 tools/make_dungeon.py
"""
import math, os, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = random.Random(11)

MT = ["FLOOR", "FLOOR2", "WALL", "FACE", "DOWN", "UP", "CHEST", "OPEN", "CAGE", "FREED", "POTION"]
PAL = {"FLOOR": 0, "FLOOR2": 0, "WALL": 0, "FACE": 0, "DOWN": 2, "UP": 2, "CHEST": 2, "OPEN": 2, "CAGE": 3, "FREED": 3, "POTION": 1}
LVL = {"FLOOR": 1, "FLOOR2": 1, "WALL": 0, "FACE": 4, "DOWN": 5, "UP": 5, "CHEST": 5, "OPEN": 5, "CAGE": 5, "FREED": 5, "POTION": 5}   # 0 wall, 1 floor, +4: drawn as it is
# the art below is drawn with the old palette numbers; these turn it into the new blend palette (wall dark, wall, floor, floor lit)
TO_BLEND_FLOOR = {0: 1, 1: 2, 2: 3, 3: 0}
TO_BLEND_WALL = {0: 0, 1: 1, 2: 3, 3: 0}


def grid(v=0):
    return [[v] * 16 for _ in range(16)]


def art(n):
    g = grid(1)
    if n in ("FLOOR", "FLOOR2"):                # no mortar grid: worn stone with specks, a few short seams and cracks (it must not look like tiles)
        for _ in range(3 if n == "FLOOR" else 5): g[R.randrange(16)][R.randrange(16)] = 2
        for _ in range(1 if n == "FLOOR" else 2):
            x, y = R.randrange(11), R.randrange(16)
            for k in range(R.randrange(2, 5)): g[y][x + k] = 0
    elif n == "WALL":                            # the top of a wall: dark, with a lit rim
        g = grid(0)
        for _ in range(10): g[R.randrange(16)][R.randrange(16)] = 1
    elif n == "FACE":                            # a wall seen from the front: bricks
        g = grid(1)
        for y in range(16):
            for x in range(16):
                if y % 5 == 4 or (x + (4 if (y // 5) % 2 else 0)) % 8 == 0: g[y][x] = 3
        for x in range(16): g[0][x] = 2; g[15][x] = 0
    elif n in ("DOWN", "UP"):                    # a stair set into the floor: a dark opening with a few thin steps
        g = grid(0)
        for k in range(4):
            y = (3 + k * 3) if n == "DOWN" else (12 - k * 3)
            for x in range(2 + k, 14 - k): g[y][x] = 2 if n == "UP" else 1
            for x in range(2 + k, 14 - k): g[y + 1][x] = 1 if n == "UP" else 0
        for y in range(1, 15): g[y][1] = g[y][14] = 1
        for x in range(1, 15): g[1][x] = g[14][x] = 1
    elif n in ("CHEST", "OPEN"):                 # a rounded treasure chest, lit from above, on a dark rug
        g = grid(0)
        for y in range(16):
            for x in range(16):
                if (x * 7 + y * 3) % 5 == 0 and (x < 2 or x > 13 or y > 13 or y < 2): g[y][x] = 0
        def inb(x, y): return 2 <= x <= 13 and 4 <= y <= 13 and not ((x in (2, 13)) and (y in (4, 13)))
        for y in range(4, 14):
            for x in range(2, 14):
                if inb(x, y): g[y][x] = 0 if (x in (2, 13) or y in (4, 13)) else 1
        for x in range(3, 13): g[5][x] = 2                      # lid highlight
        for x in range(3, 13): g[8][x] = 0 if n == "CHEST" else 1
        for y in range(5, 13): g[y][5] = g[y][10] = 2             # gold straps
        if n == "CHEST":
            g[8][7] = g[8][8] = g[9][7] = g[9][8] = 3
            g[6][3] = g[6][4] = 3
        else:
            for y in range(5, 8):
                for x in range(3, 13): g[y][x] = 0
            g[6][6] = g[5][8] = 3; g[6][9] = 2
    elif n in ("CAGE", "FREED"):
        g = grid(0)
        if n == "CAGE":                          # a little caged friend: a round bug in the friend's colour
            for y in range(6, 13):
                for x in range(4, 12):
                    if (x - 7.5) ** 2 + ((y - 9.5) * 1.3) ** 2 < 14: g[y][x] = 3
            g[8][6] = g[8][9] = 0
        for x in range(1, 15, 3):
            for y in range(2, 15): g[y][x] = 1 if g[y][x] != 3 or n == "FREED" else 3
        for x in range(16): g[1][x] = 2; g[14][x] = 2
        if n == "FREED":
            for y in range(2, 14):
                for x in range(16):
                    if x % 3 != 1: g[y][x] = 0
    elif n == "POTION":
        g = art("FLOOR")
        for y in range(5, 14):
            for x in range(5, 11): g[y][x] = 3 if y > 7 else 2
        g[4][6] = g[4][7] = g[4][8] = g[4][9] = 1; g[9][6] = 2
        for y in range(5, 14): g[y][5] = g[y][10] = 0
    return g


# monsters: 16 wide, 16 tall, 0 clear 1 dark 2 body 3 light; two frames (legs move)
SPIDER = ["................", "................", "1..1........1..1", ".1..1......1..1.", "..1..111111..1..", "...1122222211...",
          "1..12233322221..", ".1122232232221..", "..122222222221.1", ".1.1222222221.1.", "1...11211211...1", "...1..1..1..1...",
          "..1...1..1...1..", "................", "................", "................"]
BEETLE = ["................", "......1111......", "....11222211....", "...1222332221...", "..122233332221..", "..122222222221..",
          ".12222222222221.", ".12232222232221.", ".12222211222221.", ".12222211222221.", "..122221122221..", "...1122112211...",
          "..1.1..11..1.1..", ".1..1......1..1.", "................", "................"]
WASP = ["................", "..333......333..", ".33333....33333.", ".33333.11.33333.", "..333.1221.333..", ".....122221.....",
        "......1111......", ".....122221.....", "......1111......", ".....122221.....", "......1111......", "......1221......",
        ".......11.......", ".......1........", "................", "................"]
SLIME = ["................", "................", "................", "................", "......1111......", "....11222211....",
         "...1222222221...", "..122332222221..", "..123122221221..", ".12221222222221.", ".12222211122221.", ".12222222222221.",
         "..111111111111..", "................", "................", "................"]
MONS = {"SPIDER": SPIDER, "BEETLE": BEETLE, "WASP": WASP, "SLIME": SLIME}
MON_PALS = [  # 5-bit RGB: clear, dark, body, light
    [(0, 0, 0), (4, 2, 6), (16, 6, 22), (28, 20, 31)],     # 0 spider (and the queen)
    [(0, 0, 0), (2, 6, 4), (6, 18, 10), (18, 30, 16)],     # 1 beetle
    [(0, 0, 0), (6, 4, 2), (30, 24, 4), (28, 30, 31)],     # 2 wasp (wings light)
    [(0, 0, 0), (2, 8, 12), (8, 24, 22), (24, 31, 30)],    # 3 slime (and the bite flash)
]
BITE = ["................", "..3.........3...", "...3.......3....", "....3.....3.....", ".....3...3......", "......3.3.......",
        "................", "......3.3.......", ".....3...3......", "....3.....3.....", "...3.......3....", "..3.........3...",
        "................", "................", "................", "................"]


def frame2(rows):                                # the second frame: legs shuffled one pixel
    out = []
    for y, r in enumerate(rows):
        out.append((r[1:] + ".") if (y >= 9 and y % 2) else (("." + r[:-1]) if y >= 9 else r))
    return out


def queen():                                     # the spider queen: 32x32, a crown
    g = [["."] * 32 for _ in range(32)]
    for y in range(32):
        for x in range(32):
            d = ((x - 15.5) ** 2) / 120 + ((y - 17) ** 2) / 70
            if d < 1: g[y][x] = "2" if d < 0.82 else "1"
    for (cx, cy) in ((11, 13), (20, 13)):
        for dy in range(-2, 3):
            for dx in range(-2, 3):
                if dx * dx + dy * dy <= 4: g[cy + dy][cx + dx] = "3"
        g[cy][cx] = "1"
    for x in range(8, 24, 2): g[22][x] = "3"
    for k in range(4):                           # legs
        for t in range(8):
            for sx in (-1, 1):
                x = int(15.5 + sx * (12 + t)); y = 10 + k * 5 + t // 2
                if 0 <= x < 32 and 0 <= y < 32: g[y][x] = "1"
    for x in range(10, 22): g[4][x] = "3"        # the crown
    for x in (10, 13, 16, 19, 21): g[2][x] = g[3][x] = "3"
    return ["".join(r) for r in g]


DIGITS = {  # 5x7
    "0": ["01110", "10011", "10101", "10101", "11001", "10001", "01110"], "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00110", "01000", "10000", "11111"], "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"], "5": ["11111", "10000", "11110", "00001", "00001", "10001", "01110"],
    "6": ["00110", "01000", "10000", "11110", "10001", "10001", "01110"], "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"], "9": ["01110", "10001", "10001", "01111", "00001", "00010", "01100"]}


def digit_rows(ch):                              # an 8x16 sprite: the digit in gold with a dark outline, at the top
    g = [[0] * 8 for _ in range(16)]
    for y, row in enumerate(DIGITS[ch]):
        for x, c in enumerate(row):
            if c == "1": g[y + 1][x + 1] = 3 if y < 2 else 2
    for y in range(16):
        for x in range(8):
            if g[y][x] == 0 and any(0 <= y + dy < 16 and 0 <= x + dx < 8 and g[y + dy][x + dx] > 1 for dy in (-1, 0, 1) for dx in (-1, 0, 1)): g[y][x] = 1
    return ["".join(".123"[v] for v in r) for r in g]


def poof(r, seed):                               # a burst of smoke and sparks, 16x16, in the monster's own colours
    import random
    rnd = random.Random(seed); g = [[0] * 16 for _ in range(16)]
    for k in range(7):
        a = k * math.pi * 2 / 7 + rnd.random() * 0.5; cx = 7.5 + math.cos(a) * r; cy = 7.5 + math.sin(a) * r; rr = 2.6 - r * 0.15
        for y in range(16):
            for x in range(16):
                d = math.hypot(x - cx, y - cy)
                if d < rr: g[y][x] = max(g[y][x], 3 if d < rr * 0.45 else 2)
                elif d < rr + 0.9 and g[y][x] == 0: g[y][x] = 1
    for k in range(5):
        x, y = rnd.randrange(1, 15), rnd.randrange(1, 15)
        if g[y][x] == 0: g[y][x] = 3
    return ["".join(".123"[v] for v in r) for r in g]


def tile2bpp(g, x0, y0):
    out = []
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            v = g[y0 + y][x0 + x]
            lo |= (v & 1) << (7 - x); hi |= ((v >> 1) & 1) << (7 - x)
        out += [lo, hi]
    return out


def spr_tiles(rows, w, h):                       # 8x16 sprites, column by column (then row by row of 16)
    g = [[".123".index(c) for c in r] for r in rows]
    t = []
    for row in range(h // 16):
        for col in range(w // 8):
            t += tile2bpp(g, col * 8, row * 16) + tile2bpp(g, col * 8, row * 16 + 8)
    return t


KINDS = [  # stone (shadow, wall, lit, mortar), floor (dark, floor, lit, crack)
    ("GARDEN", [(4, 5, 8), (10, 12, 16), (18, 20, 24), (6, 7, 10)], [(6, 5, 4), (13, 11, 8), (18, 16, 12), (8, 7, 5)]),
    ("DESERT", [(8, 5, 3), (18, 12, 7), (26, 20, 12), (12, 8, 4)], [(10, 7, 4), (20, 15, 9), (26, 21, 13), (13, 9, 5)]),
    ("ICE",    [(4, 7, 12), (12, 18, 26), (22, 27, 31), (8, 12, 18)], [(8, 11, 16), (16, 21, 27), (24, 28, 31), (10, 14, 20)]),
    ("JUNGLE", [(3, 6, 3), (8, 14, 8), (14, 21, 12), (5, 9, 5)], [(5, 6, 3), (11, 12, 7), (16, 17, 10), (7, 8, 4)]),
    ("OCEAN",  [(2, 6, 9), (6, 13, 17), (12, 21, 24), (4, 9, 12)], [(4, 7, 8), (10, 14, 15), (16, 20, 20), (6, 9, 10)]),
    ("DUSK",   [(5, 3, 7), (11, 7, 15), (18, 13, 22), (7, 4, 10)], [(6, 4, 7), (12, 9, 14), (17, 14, 19), (8, 6, 9)]),
]
GOLD = [(4, 2, 1), (16, 9, 3), (30, 24, 6), (31, 31, 24)]
CAGE = [(3, 3, 5), (14, 14, 18), (26, 26, 30), (30, 12, 10)]


def hn(a, b, c):
    return ((a * 73856093) ^ (b * 19349663) ^ (c * 83492791)) & 0xFFFF


def blend_quarter(pat):
    """Top-left quarter of a cell where wall (0) meets floor (1): pat bits 0..3 = the cell / left / upper / diagonal neighbour is floor.
    0 wall, 1 shadow on the floor next to the wall, 2 floor, 3 lit speck."""
    his = [(pat >> b) & 1 for b in range(4)]
    cs = [(8, 8), (-8, 8), (8, -8), (-8, -8)]
    g = [[0] * 8 for _ in range(8)]
    for y in range(8):
        for x in range(8):
            X, Y = x + 0.5, y + 0.5
            num = den = 0.0
            for h, (cx, cy) in zip(his, cs):
                w = 1.0 / (((X - cx) ** 2 + (Y - cy) ** 2) + 40.0) ** 0.8
                num += w * h; den += w
            v = num / den
            j = ((hn(x, y, pat) & 255) / 255 - .5) * 0.12
            if v + j > 0.5:
                g[y][x] = 1 if v + j < 0.62 else (3 if (hn(x, y, 50 + pat) & 255) < 14 else 2)
            else:
                g[y][x] = 1 if (hn(x, y, 99 + pat) & 255) < 22 else 0
    return g


def blend_pal(stone, floor):
    return [stone[0], stone[1], floor[1], floor[2]]


def main():
    arts = {n: art(n) for n in MT}
    for n in ("FLOOR", "FLOOR2"): arts[n] = [[TO_BLEND_FLOOR[v] for v in r] for r in arts[n]]
    for n in ("WALL", "FACE"): arts[n] = [[TO_BLEND_WALL[v] for v in r] for r in arts[n]]
    tiles = []
    for n in MT:
        for (x0, y0) in ((0, 0), (8, 0), (0, 8), (8, 8)): tiles += tile2bpp(arts[n], x0, y0)
    for pat in range(1, 15):                       # the 14 blend tiles
        q = blend_quarter(pat)
        tiles += tile2bpp([[q[y][x] if x < 8 and y < 8 else 0 for x in range(16)] for y in range(16)], 0, 0)
    st = []                                        # sprite tiles: each monster frame 0 + frame 1 (4 tiles each), then the bite, then the queen
    for k in ("SPIDER", "BEETLE", "WASP", "SLIME"):
        st += spr_tiles(MONS[k], 16, 16) + spr_tiles(frame2(MONS[k]), 16, 16)
    st += spr_tiles(BITE, 16, 16)
    q = queen(); st += spr_tiles(q, 32, 32)
    q2 = [r if y < 20 else ("." + r[:-1]) for y, r in enumerate(q)]; st += spr_tiles(q2, 32, 32)
    st += spr_tiles(poof(3.0, 1), 16, 16) + spr_tiles(poof(5.5, 2), 16, 16)
    for ch in "0123456789": st += spr_tiles(digit_rows(ch), 8, 16)
    c = ["#pragma bank 5", "/* generated by tools/make_dungeon.py: do not edit */", "#include <gb/gb.h>", "#include <gb/cgb.h>", '#include "dungeon_art.h"']
    g = ["#pragma bank 11", "/* generated by tools/make_dungeon.py: do not edit. The dungeon's tiles, apart from its code (bank 5 was full) */",
         "#include <gb/gb.h>", '#include "dungeon_art.h"']
    g.append("static const UINT8 dg_tiles[DG_NTILES * 16] = {" + ",".join("0x%02X" % b for b in tiles) + "};")
    g.append("static const UINT8 mon_tiles[MON_NTILES * 16] = {" + ",".join("0x%02X" % b for b in st) + "};")
    g.append("void dungeon_tiles_load(UINT8 mon0) BANKED { set_bkg_data(0, DG_NTILES, dg_tiles); set_sprite_data(mon0, MON_NTILES, mon_tiles); }")
    open(os.path.join(ROOT, "src", "dungeon_gfx.c"), "w").write("\n".join(g) + "\n")
    c.append("const UINT8 dg_mtpal[DG_NMT] = {" + ",".join(str(PAL[n]) for n in MT) + "};")
    c.append("const UINT8 dg_mtlvl[DG_NMT] = {" + ",".join(str(LVL[n]) for n in MT) + "};")
    pals = []
    for kname, stone, floor in KINDS:
        for p in (blend_pal(stone, floor), floor, GOLD, CAGE): pals += ["RGB(%d,%d,%d)" % col for col in p]
    c.append("const palette_color_t dg_pals[DG_NKIND * 16] = {" + ",".join(pals) + "};")
    c.append("const palette_color_t mon_pals[16] = {" + ",".join("RGB(%d,%d,%d)" % col for p in MON_PALS for col in p) + "};")
    open(os.path.join(ROOT, "src", "dungeon_art.c"), "w").write("\n".join(c) + "\n")
    h = ["#ifndef DUNGEON_ART_H", "#define DUNGEON_ART_H", "#include <gb/gb.h>", "#include <gb/cgb.h>",
         "/* generated by tools/make_dungeon.py: the dungeon's tiles and monsters (bank 5) */",
         "enum { " + ", ".join("D_" + n for n in MT) + ", DG_NMT };",
         "#define DG_NTILES (DG_NMT * 4 + 14)      /* the metatile pictures, then 14 wall/floor blend tiles (index DG_NMT * 4 + pat - 1) */",
         "#define DG_NKIND %d" % len(KINDS),
         "#define MON_T_SMALL(kind, frame) (((kind) * 2 + (frame)) * 4)   /* 16x16 monster: 4 tiles (two 8x16 sprites) */",
         "#define MON_T_BITE 32",
         "#define MON_T_QUEEN(frame) (36 + (frame) * 16)                 /* the queen: 16 tiles (eight 8x16 sprites) */",
         "#define MON_T_POOF(frame) (68 + (frame) * 4)                  /* a burst when a monster dies, 16x16 */",
         "#define MON_T_DIGIT(d) (76 + (d) * 2)                         /* damage numbers: one 8x16 sprite per digit (palette 2: gold) */",
         "#define MON_NTILES %d" % (len(st) // 16),
         "void dungeon_tiles_load(UINT8 mon0) BANKED;   /* (dungeon_gfx.c, bank 11) floor tiles at 0 and monsters at mon0, in the selected VRAM bank */",
         "extern const UINT8 dg_mtpal[DG_NMT];",
         "extern const UINT8 dg_mtlvl[DG_NMT];       /* 0 wall, 1 floor, +4 = drawn as it is */",
         "extern const palette_color_t dg_pals[DG_NKIND * 16];",
         "extern const palette_color_t mon_pals[16];",
         "#endif"]
    open(os.path.join(ROOT, "src", "dungeon_art.h"), "w").write("\n".join(h) + "\n")
    try:
        from PIL import Image
        S = 3; W = (len(MT) * 18) * S
        im = Image.new("RGB", (W, (len(KINDS) * 18 + 60) * S), (40, 40, 40))
        for ki, (kn, stone, floor) in enumerate(KINDS):
            pals = [blend_pal(stone, floor), floor, GOLD, CAGE]
            for mi, n in enumerate(MT):
                for y in range(16):
                    for x in range(16):
                        r, g, b = pals[PAL[n]][arts[n][y][x]]
                        for yy in range(S):
                            for xx in range(S): im.putpixel(((mi * 18 + x) * S + xx, (ki * 18 + y) * S + yy), (r * 8, g * 8, b * 8))
        y0 = len(KINDS) * 18 + 4
        for i, (name, rows, p) in enumerate((("SPIDER", SPIDER, 0), ("BEETLE", BEETLE, 1), ("WASP", WASP, 2), ("SLIME", SLIME, 3), ("BITE", BITE, 3))):
            for y in range(16):
                for x in range(16):
                    v = ".123".index(rows[y][x])
                    if v:
                        r, g, b = MON_PALS[p][v]
                        for yy in range(S):
                            for xx in range(S): im.putpixel(((i * 18 + x) * S + xx, (y0 + y) * S + yy), (r * 8, g * 8, b * 8))
        for y in range(32):
            for x in range(32):
                v = ".123".index(q[y][x])
                if v:
                    r, g, b = MON_PALS[0][v]
                    for yy in range(S):
                        for xx in range(S): im.putpixel(((100 + x) * S + xx, (y0 + y) * S + yy), (r * 8, g * 8, b * 8))
        im.save(os.path.join(ROOT, "tools", "dungeon_check.png"))
    except ImportError:
        pass
    print("dungeon art: %d metatiles, %d sprite tiles" % (len(MT), len(st) // 16))


if __name__ == "__main__":
    main()
