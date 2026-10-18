#!/usr/bin/env python3
"""BUGSTAR GBC space art (step 8).

Writes src/space_art.c + src/space_art.h and tools/space_check.png.

Ground (BG, VRAM bank 1, 16x16 metatiles): four starfield tiles (palette 4) and a planet 32x32 cut into four metatiles
(TL TR BL BR); the planet is drawn in palette 5, 6 or 7, each tinted for the kind of world it is (two of the six kinds per palette
would clash, so the space screen sets palettes 5..7 for the three kinds nearest the ship: see space.c).
Sprites (VRAM bank 1): the ship in 8 directions (16x16), a pirate drone (2 frames), a laser bolt, an explosion (2 frames) and the
arrow that points at the nearest star (8 directions). Sprite palettes 0..3: ship, pirate, laser + arrow, explosion.

Run:  python3 tools/make_space.py
"""
import math, os, random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = random.Random(5)


def blank(w=16, h=16): return [[0] * w for _ in range(h)]


def stars(n, big):
    g = blank()
    for _ in range(n): g[R.randrange(16)][R.randrange(16)] = 1
    for _ in range(big):
        x, y = R.randrange(1, 15), R.randrange(1, 15); g[y][x] = 3; g[y][x - 1] = g[y][x + 1] = g[y - 1][x] = g[y + 1][x] = 2
    return g


def planet():
    g = blank(32, 32)
    for y in range(32):
        for x in range(32):
            d = math.hypot(x - 15.5, y - 15.5)
            if d < 14.5:
                lit = ((x - 10) ** 2 + (y - 10) ** 2) ** 0.5
                g[y][x] = 3 if lit < 6 else 2 if lit < 15 else 1
            if 14.5 <= d < 15.6: g[y][x] = 1
    for k in range(40):                                         # continents: blotches of colour 2/3 swapped
        x, y = R.randrange(4, 28), R.randrange(4, 28)
        if math.hypot(x - 15.5, y - 15.5) < 12 and g[y][x] > 1: g[y][x] = 5 - g[y][x]
    return g


SHIP16 = [".......11.......", "......1331......", "......1331......", ".....123321.....", ".....122221.....", "....12233221....",
          "11..12222221..11", "121122333322112.", "1222223333222221", "1222222222222221", ".12222222222221.", "..122211112221..",
          "...121....121...", "....1......1....", "................", "................"]
PIRATE = ["................", "....11....11....", "...1221..1221...", "...12221122221..", "....1222222221..", "...122233332221.",
          "..1223311332221.", "..1223111132221.", "..1223311332221.", "...122233332221.", "....1222222221..", "...12221122221..",
          "...1221..1221...", "....11....11....", "................", "................"]
LASER = ["................", "................", "................", "................", "......3333......", ".....322223.....",
         "......3333......", "................"] + ["." * 16] * 8
ARROW = ["................", "................", ".......33.......", "......3223......", ".....322223.....", "....32222223....",
         ".......22.......", ".......22.......", ".......22.......", "................"] + ["." * 16] * 6


def boom(r):
    g = blank()
    for y in range(16):
        for x in range(16):
            d = math.hypot(x - 7.5, y - 7.5) + R.random() * 1.5
            if d < r: g[y][x] = 3 if d < r * 0.4 else 2 if d < r * 0.75 else 1
    return g


def beast():                                                # the Technobeast, 48x48: an armoured shell on six legs, a head with red eyes, a hole for its core
    g = [[0] * 48 for _ in range(48)]
    def line(x0, y0, x1, y1, c, w=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        for i in range(n + 1):
            x = x0 + (x1 - x0) * i / n; y = y0 + (y1 - y0) * i / n
            for dy in range(-w + 1, w):
                for dx in range(-w + 1, w):
                    xi, yi = int(round(x + dx)), int(round(y + dy))
                    if 0 <= xi < 48 and 0 <= yi < 48: g[yi][xi] = c
    for side in (-1, 1):                                    # legs: out, then down to a claw
        for k, (ay, ex, ey, fy) in enumerate(((18, 21, 10, 4), (25, 23, 26, 33), (32, 20, 40, 46))):
            x0 = 24 + side * 12; x1 = 24 + side * ex; x2 = 24 + side * min(23, ex + 1)
            line(x0, ay, x1, ey, 1, 2); line(x1, ey, x2, fy, 1, 2)
            line(x0, ay, x1, ey, 2, 1); line(x1, ey, x2, fy, 2, 1)
    for y in range(48):
        for x in range(48):
            d = ((x - 23.5) / 15.5) ** 2 + ((y - 25) / 15.5) ** 2
            if d < 1: g[y][x] = 1 if d > 0.82 else 2
            h = ((x - 23.5) / 9) ** 2 + ((y - 8) / 6.5) ** 2   # the head
            if h < 1: g[y][x] = 1 if h > 0.7 else 2
    for a in range(8):                                     # armour seams
        ang = a * math.pi / 4 + math.pi / 8
        line(23.5 + math.cos(ang) * 8, 25 + math.sin(ang) * 8, 23.5 + math.cos(ang) * 14, 25 + math.sin(ang) * 14, 1)
    for (x, y) in ((20, 7), (21, 7), (26, 7), (27, 7), (20, 8), (27, 8)): g[y][x] = 3      # eyes
    for (x, y) in ((9, 25), (38, 25), (14, 36), (33, 36), (14, 14), (33, 14)): g[y][x] = 3  # running lights
    line(19, 13, 17, 16, 1); line(28, 13, 30, 16, 1)       # mandibles
    for y in range(48):
        for x in range(48):
            if math.hypot(x - 23.5, y - 23.5) < 7.5: g[y][x] = 0      # the core shows through here
            elif math.hypot(x - 23.5, y - 23.5) < 8.6: g[y][x] = 1
    return g


def core(open_):
    g = blank()
    for y in range(16):
        for x in range(16):
            d = math.hypot(x - 7.5, y - 7.5)
            if open_:
                if d < 7.6: g[y][x] = 3 if d < 2.6 else 2 if d < 5 else 1
            else:
                if d < 7.6: g[y][x] = 1 if (d > 6.6 or abs(x - 7.5) < 1 or abs(y - 7.5) < 1 or abs(abs(x - 7.5) - abs(y - 7.5)) < 0.8) else 2
    return g


def orb():  # an 8x16 sprite: a hot ball in the top 8 rows
    g = [[0] * 8 for _ in range(16)]
    for y in range(8):
        for x in range(8):
            d = math.hypot(x - 3.5, y - 3.5)
            if d < 3.6: g[y][x] = 3 if d < 1.6 else 2 if d < 2.8 else 1
    return g


def parse(rows): return [[".123".index(c) for c in r.ljust(16, ".")[:16]] for r in rows]


def rotate(g, ang):                                         # nearest-neighbour rotation of a 16x16 picture about its centre
    out = blank(); c, s = math.cos(ang), math.sin(ang)
    for y in range(16):
        for x in range(16):
            dx, dy = x - 7.5, y - 7.5
            sx, sy = c * dx + s * dy + 7.5, -s * dx + c * dy + 7.5
            ix, iy = int(round(sx)), int(round(sy))
            if 0 <= ix < 16 and 0 <= iy < 16: out[y][x] = g[iy][ix]
    return out


def tile2bpp(g, x0, y0):
    out = []
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            v = g[y0 + y][x0 + x]
            lo |= (v & 1) << (7 - x); hi |= ((v >> 1) & 1) << (7 - x)
        out += [lo, hi]
    return out


def mt_tiles(g):  # a 16x16 metatile: TL TR BL BR
    return sum((tile2bpp(g, x0, y0) for (x0, y0) in ((0, 0), (8, 0), (0, 8), (8, 8))), [])


def spr16(g):     # a 16x16 sprite: two 8x16 columns
    return tile2bpp(g, 0, 0) + tile2bpp(g, 0, 8) + tile2bpp(g, 8, 0) + tile2bpp(g, 8, 8)


# world kinds (same order as the planets): a planet's three colours, seen from space
KIND_COLS = [[(4, 10, 20), (8, 20, 6), (24, 30, 16)], [(16, 9, 3), (29, 21, 10), (31, 29, 20)], [(10, 16, 26), (24, 28, 31), (31, 31, 31)],
             [(2, 10, 4), (5, 20, 6), (14, 28, 10)], [(2, 6, 22), (6, 16, 30), (22, 30, 16)], [(8, 2, 12), (16, 8, 22), (28, 18, 28)]]
SPACE_PAL = [(0, 0, 3), (10, 10, 16), (20, 20, 28), (31, 31, 31)]
SPR_PALS = [[(0, 0, 0), (6, 4, 8), (26, 6, 8), (31, 24, 20)],      # 0 ship
            [(0, 0, 0), (4, 6, 4), (12, 18, 12), (30, 30, 6)],     # 1 pirate
            [(0, 0, 0), (0, 0, 0), (31, 31, 12), (31, 16, 4)],     # 2 laser, arrow
            [(0, 0, 0), (20, 4, 2), (31, 18, 4), (31, 31, 20)],    # 3 explosion, the beast's orbs
            [(0, 0, 0), (3, 3, 7), (12, 13, 18), (31, 8, 6)],      # 4 the beast
            [(0, 0, 0), (16, 2, 18), (31, 10, 24), (31, 28, 31)]]  # 5 its core


def main():
    star_mts = [stars(2, 0), stars(3, 0), stars(1, 1), stars(4, 1)]
    p = planet()
    bg = []
    for g in star_mts: bg += mt_tiles(g)
    for (x0, y0) in ((0, 0), (16, 0), (0, 16), (16, 16)):
        sub = [row[x0:x0 + 16] for row in p[y0:y0 + 16]]
        bg += mt_tiles(sub)
    ship = parse(SHIP16)
    st = []
    for d in range(8): st += spr16(rotate(ship, -d * math.pi / 4))      # 0 = up, then clockwise
    pir = parse(PIRATE); st += spr16(pir) + spr16([row[1:] + [0] for row in pir])
    st += spr16(parse(LASER))
    st += spr16(boom(5)) + spr16(boom(8))
    arr = parse(ARROW)
    for d in range(8): st += spr16(rotate(arr, -d * math.pi / 4))
    bst = beast(); nb = len(st) // 16
    for cx in range(6):
        for cy in range(3): st += tile2bpp(bst, cx * 8, cy * 16) + tile2bpp(bst, cx * 8, cy * 16 + 8)   # 18 8x16 sprites, column by column
    st += spr16(core(1)) + spr16(core(0))
    ob = orb(); st += tile2bpp(ob, 0, 0) + tile2bpp(ob, 0, 8)
    c = ["#pragma bank 6", "/* generated by tools/make_space.py: do not edit */", "#include <gb/gb.h>", "#include <gb/cgb.h>", '#include "space_art.h"']
    c.append("const UINT8 sp_bg[SP_NMT * 64] = {" + ",".join("0x%02X" % b for b in bg) + "};")
    c.append("const UINT8 sp_spr[SP_NSPR * 16] = {" + ",".join("0x%02X" % b for b in st) + "};")
    c.append("const palette_color_t sp_space_pal[4] = {" + ",".join("RGB(%d,%d,%d)" % col for col in SPACE_PAL) + "};")
    c.append("const palette_color_t sp_kind_pal[6 * 4] = {" + ",".join("RGB(0,0,3)," + ",".join("RGB(%d,%d,%d)" % col for col in k) for k in KIND_COLS) + "};")
    perm = list(range(256)); random.Random(77).shuffle(perm)
    c.append("const UINT8 sp_perm[256] = {" + ",".join(str(v) for v in perm) + "};")
    c.append("const palette_color_t sp_spr_pal[24] = {" + ",".join("RGB(%d,%d,%d)" % col for p2 in SPR_PALS for col in p2) + "};")
    open(os.path.join(ROOT, "src", "space_art.c"), "w").write("\n".join(c) + "\n")
    h = ["#ifndef SPACE_ART_H", "#define SPACE_ART_H", "#include <gb/gb.h>", "#include <gb/cgb.h>",
         "/* generated by tools/make_space.py: space (bank 6) */",
         "enum { SP_STAR0, SP_STAR1, SP_STAR2, SP_STAR3, SP_PL_TL, SP_PL_TR, SP_PL_BL, SP_PL_BR, SP_NMT };",
         "#define SP_T_SHIP(d)   ((d) * 4)            /* 16x16 sprites: 4 tiles each (two 8x16 columns) */",
         "#define SP_T_PIRATE(f) (32 + (f) * 4)",
         "#define SP_T_LASER     40",
         "#define SP_T_BOOM(f)   (44 + (f) * 4)",
         "#define SP_T_ARROW(d)  (52 + (d) * 4)",
         "#define SP_T_BEAST(cx, cy) (84 + ((cx) * 3 + (cy)) * 2)   /* the Technobeast: 6 columns x 3 rows of 8x16 sprites */",
         "#define SP_T_CORE(open) (120 + ((open) ? 0 : 4))",
         "#define SP_T_ORB       128",
         "#define SP_NSPR        %d" % (len(st) // 16),
         "extern const UINT8 sp_bg[SP_NMT * 64];",
         "extern const UINT8 sp_spr[SP_NSPR * 16];",
         "extern const palette_color_t sp_space_pal[4];",
         "extern const palette_color_t sp_kind_pal[6 * 4];",
         "extern const palette_color_t sp_spr_pal[24];   /* sprite palettes 0..5 */",
         "extern const UINT8 sp_perm[256];               /* a shuffle of 0..255, for the starfield hash */",
         "#endif"]
    open(os.path.join(ROOT, "src", "space_art.h"), "w").write("\n".join(h) + "\n")
    try:
        from PIL import Image
        S = 3
        im = Image.new("RGB", (18 * 10 * S, 18 * 4 * S), (40, 40, 40))
        def put(g, ox, oy, pal):
            for y in range(len(g)):
                for x in range(len(g[0])):
                    v = g[y][x]
                    if v or pal is SPACE_PAL:
                        r, gg, b = pal[v]
                        for yy in range(S):
                            for xx in range(S): im.putpixel(((ox + x) * S + xx, (oy + y) * S + yy), (r * 8, gg * 8, b * 8))
        for i, g in enumerate(star_mts): put(g, i * 18, 0, SPACE_PAL)
        put(p, 4 * 18, 0, [(0, 0, 3)] + KIND_COLS[0])
        put(p, 6 * 18, 0, [(0, 0, 3)] + KIND_COLS[1])
        for d in range(8): put(rotate(ship, -d * math.pi / 4), d * 18, 36, SPR_PALS[0])
        put(pir, 0, 54, SPR_PALS[1]); put(parse(LASER), 18, 54, SPR_PALS[2]); put(boom(8), 36, 54, SPR_PALS[3])
        for d in range(8): put(rotate(arr, -d * math.pi / 4), 54 + d * 14, 54, SPR_PALS[2])
        im2 = Image.new("RGB", (18 * 10 * S, 18 * 4 * S + 52 * S), (40, 40, 40)); im2.paste(im, (0, 0)); im = im2
        put(bst, 2, 74, SPR_PALS[4]); put(core(1), 18, 90, SPR_PALS[5]); put(core(0), 70, 90, SPR_PALS[4]); put(ob, 90, 90, SPR_PALS[3])
        put(bst, 110, 74, SPR_PALS[4]); put(core(1), 126, 90, SPR_PALS[5])
        im.save(os.path.join(ROOT, "tools", "space_check.png"))
    except ImportError:
        pass
    print("space art: %d bg metatiles, %d sprite tiles" % (8, len(st) // 16))


if __name__ == "__main__":
    main()
