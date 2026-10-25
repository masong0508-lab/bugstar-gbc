#!/usr/bin/env python3
"""BUGSTAR GBC - art for the 3D space view (src/space3d_art.c/.h, bank 18).
Sprites are 8x16 (tile pairs: top, bottom). Planets are shaded discs rendered from a sphere, at 4 sizes, 2 looks:
 dot (8), 16x16 (2 sprites), 32x32 (8 sprites), 48x48 (18 sprites). Index 0 is transparent, 1..3 dark..light (the kind palette)."""
import math, sys

BAYER = [[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]]
def render_disc(D, variant):
    px = [[0]*D for _ in range(D)]
    c = (D - 1) / 2.0; r = D / 2.0
    L = (-0.45, -0.55, 0.70); n = math.sqrt(sum(v*v for v in L)); L = tuple(v/n for v in L)
    for j in range(D):
        for i in range(D):
            x = (i - c) / r; y = (j - c) / r
            q = x*x + y*y
            if q > 1.0: continue
            z = math.sqrt(1.0 - q)
            I = max(0.0, x*L[0] + y*L[1] + z*L[2]) * 0.80 + 0.02
            lon = math.atan2(x, z) + (0.8 if variant else 0.0); lat = math.asin(max(-1, min(1, -y)))
            if variant == 0:
                t = math.sin(2.2*lon + 1.6*math.sin(1.7*lat + 0.4)) + 0.6*math.sin(3.7*lat + 1.1*lon + 2.0)
                if t > 0.40: I += 0.20
                elif t < -0.45: I -= 0.16
                if abs(lat) > 1.25: I += 0.12
            else:
                t = math.sin(lat*9.0 + 0.9*math.sin(lon*3.0 + lat*2.0))
                I += 0.17 * t
                sx, sy = lon - 0.5, lat + 0.35                    # a storm
                if sx*sx*2.2 + sy*sy*5.0 < 0.06: I += 0.28
            I = max(0.0, min(1.0, I))
            b = (BAYER[j % 4][i % 4] + 0.5) / 16.0
            f = I * 3.0 - 0.15 + (b - 0.5) * 0.8
            px[j][i] = 1 + max(0, min(2, int(f)))
    return px

def tile_bytes(rows):                       # rows: 8 lists of 8 values 0..3
    out = []
    for row in rows:
        lo = hi = 0
        for v in row:
            lo = (lo << 1) | (v & 1); hi = (hi << 1) | ((v >> 1) & 1)
        out += [lo, hi]
    return out

def blank(w, h): return [[0]*w for _ in range(h)]
def sprite_pairs(img, cols, rows):          # img: (rows*16) x (cols*8). -> tile list, sprite (r, c) = tiles 2*(r*cols+c), +1
    tiles = []
    for r in range(rows):
        for c in range(cols):
            for half in (0, 1):
                y0 = r*16 + half*8; x0 = c*8
                tiles.append(tile_bytes([img[y0+y][x0:x0+8] for y in range(8)]))
    return tiles

def paste(dst, src, ox, oy):
    for j, row in enumerate(src):
        for i, v in enumerate(row):
            if v: dst[oy+j][ox+i] = v

tiles = []; names = {}
def add(name, tl): names[name] = len(tiles); tiles.extend(tl)

# the planets: [variant][size]
for v in (0, 1):
    d8 = render_disc(6, v); img = blank(8, 16); paste(img, d8, 1, 1); add("DOT_PL%d" % v, sprite_pairs(img, 1, 1))
    d16 = render_disc(16, v); add("D16_%d" % v, sprite_pairs(d16, 2, 1))
    d32 = render_disc(32, v); add("D32_%d" % v, sprite_pairs(d32, 4, 2))
    d48 = render_disc(48, v); add("D48_%d" % v, sprite_pairs(d48, 6, 3))

# dots (top 8x8 of a 8x16): tiny, small, medium
def dot(pat):
    img = blank(8, 16)
    for j, row in enumerate(pat):
        for i, ch in enumerate(row):
            if ch != '.': img[2+j][1+i] = int(ch)
    return sprite_pairs(img, 1, 1)
add("DOT1", dot(["..33..", ".3333.", ".3333.", "..33.."][:0] or ["...", ".3."][:0] or [".33.", "3333", "3333", ".33."]) if False else dot(["..", ".3"]))
add("DOT2", dot([".33.", "3333", "3333", ".33."]))
add("DOT3", dot([".2332.", "233332", "333333", "333333", "233332", ".2332."]))

# the station: small (8) and big (16)
def station(D):
    img = blank(D, D if D >= 16 else 16)
    if D == 16:
        cx, cy = 7.5, 7.5
        for j in range(16):
            for i in range(16):
                x, y = (i - cx) / 7.5, (j - cy) / 7.5
                q = x*x*1.0 + y*y*3.2
                if 0.55 < q < 1.0: img[j][i] = 2 if (i + j) % 5 else 3
                if 0.35 < q <= 0.55 and abs(x) > 0.8: img[j][i] = 1
        for j in range(5, 11):
            for i in range(5, 11):
                if (i - 7.5)**2 + (j - 7.5)**2 < 9: img[j][i] = 3 if (i < 8 and j < 8) else 2
        for i in (1, 14): img[8][i] = 3
        img[7][7] = img[7][8] = 3
        return sprite_pairs(img, 2, 1)
    s = blank(8, 16)
    for (i, j, v) in [(2,3,2),(3,3,3),(4,3,3),(5,3,2),(1,4,2),(2,4,1),(5,4,1),(6,4,2),(3,5,3),(4,5,3),(2,5,2),(5,5,2),(3,4,3),(4,4,3)]: s[j][i] = v
    return sprite_pairs(s, 1, 1)
add("ST8", station(8)); add("ST16", station(16))

# the crosshair, arrows, a down-pointing target marker
img = blank(8, 16)
for i in (0, 1, 2, 5, 6, 7): img[7][i] = 3; img[8][i] = 3
for j in (3, 4, 5, 10, 11, 12): img[j][3] = 3; img[j][4] = 3
add("CROSS", sprite_pairs(img, 1, 1))
def arrow(kind):
    img = blank(8, 16)
    pts = {
      'U': [(3,4),(4,4),(2,5),(3,5),(4,5),(5,5),(1,6),(2,6),(3,6),(4,6),(5,6),(6,6),(3,7),(4,7),(3,8),(4,8),(3,9),(4,9)],
      'D': [(3,11),(4,11),(2,10),(3,10),(4,10),(5,10),(1,9),(2,9),(3,9),(4,9),(5,9),(6,9),(3,8),(4,8),(3,7),(4,7),(3,6),(4,6)],
      'L': [(0,7),(0,8),(1,6),(1,7),(1,8),(1,9),(2,5),(2,6),(2,7),(2,8),(2,9),(2,10),(3,7),(3,8),(4,7),(4,8),(5,7),(5,8),(6,7),(6,8)],
      'R': [(7,7),(7,8),(6,6),(6,7),(6,8),(6,9),(5,5),(5,6),(5,7),(5,8),(5,9),(5,10),(4,7),(4,8),(3,7),(3,8),(2,7),(2,8),(1,7),(1,8)] }[kind]
    for (i, j) in pts: img[j][i] = 3
    for (i, j) in pts:
        pass
    return sprite_pairs(img, 1, 1)
for k in "UDLR": add("ARW_" + k, arrow(k))

assert len(tiles) % 2 == 0
# BG: empty, dim, bright, cross star (palette 3 of the space: 0 black, 1.. blues/white)
def bgt(rows): return tile_bytes(rows)
bg = []
bg.append(bgt(blank(8, 8)))
t = blank(8, 8); t[3][4] = 1; bg.append(bgt(t))
t = blank(8, 8); t[5][2] = 2; bg.append(bgt(t))
t = blank(8, 8); t[3][3] = 3; t[2][3] = 1; t[4][3] = 1; t[3][2] = 1; t[3][4] = 1; bg.append(bgt(t))

sin = [int(round(64 * math.sin(2 * math.pi * i / 256))) for i in range(256)]

c = ["/* generated by tools/make_space3d.py: the 3D space view's pictures (bank 18) */", "#pragma bank 18", '#include <gb/gb.h>', '#include "space3d_art.h"']
c.append("const UINT8 s3_tiles[S3_NTILES * 16] = {" + ",".join(str(b) for t in tiles for b in t) + "};")
c.append("const UINT8 s3_bg[4 * 16] = {" + ",".join(str(b) for t in bg for b in t) + "};")
c.append("const INT8 s3_sin[256] = {" + ",".join(str(v) for v in sin) + "};")
open("src/space3d_art.c", "w").write("\n".join(c) + "\n")
h = ["#ifndef SPACE3D_ART_H", "#define SPACE3D_ART_H", "#include <gb/gb.h>", "/* generated by tools/make_space3d.py (bank 18). Sprite tile numbers are relative to where the caller loads s3_tiles. */",
     "#define S3_NTILES %d" % len(tiles)]
for k, v in names.items(): h.append("#define S3_T_%s %d" % (k, v))
h += ["extern const UINT8 s3_tiles[S3_NTILES * 16];", "extern const UINT8 s3_bg[4 * 16];", "extern const INT8 s3_sin[256];   /* 64 * sin, 256 steps a turn */", "#endif"]
open("src/space3d_art.h", "w").write("\n".join(h) + "\n")
print("tiles", len(tiles), names)

if len(sys.argv) > 1:                       # a preview sheet
    from PIL import Image
    cols = {0: (0,0,0), 1: (40,70,160), 2: (90,170,70), 3: (230,240,170)}
    im = Image.new("RGB", (4*50+20, 56*2+10), (20, 20, 40))
    for v in (0, 1):
        for k, (D, off) in enumerate([(48,0),(32,52),(16,90),(6,110)]):
            d = render_disc(D, v)
            for j in range(D):
                for i in range(D):
                    if d[j][i]: im.putpixel((5 + off + i, 5 + v*58 + j), cols[d[j][i]])
    im.resize((im.width*4, im.height*4), Image.NEAREST).save(sys.argv[1])
