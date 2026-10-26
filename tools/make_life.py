#!/usr/bin/env python3
# Makes src/life_art.c: the animals that live on the 6 kinds of world (garden, desert, ice, jungle, ocean, dusk), 2 each.
# 8x8 pictures (1 dark, 2 body, 3 light), two frames (the second hops 1 pixel), cut into 8x16 sprites. Run: python3 tools/make_life.py [preview.png]
import sys
W = [
 [["..2..2..","..2..2..",".222222.",".212212.",".223322.",".222222.",".223322.",".11..11."],   # bunny
  ["..2222..",".222222.",".221223.",".222233.",".222222.","..2222..","..1..1..","..1..1.."]],  # chick
 [["........",".....22.","..22222.","2.222212","22222222",".2.22.2.",".1.11.1.","........"],  # lizard
  [".2....2.",".22..22.",".222222.",".212212.",".223322.","..2222..",".222222.","..1..1.."]],  # fennec
 [["..1111..",".111111.",".131131.",".112211.",".133331.",".133331.",".113311.","..22.22."],  # penguin
  ["..3..3..","..3..3..",".333333.",".313313.",".333233.","33333333",".333333.",".22..22."]],  # hare
 [["........",".22..22.",".212212.",".222222.","22322322",".222222.",".2.22.2.",".1....1."],  # frog
  ["..2222..",".222122.",".222233.",".222222.",".223322.",".223322.","..2222..","..1..1.."]],  # parrot
 [["........","2..22..2","22.22.22",".222222.",".212212.",".222222.","1.1..1.1","........"],  # crab
  ["........","..2222..",".223322.","22333322",".223322.",".222221.",".1.11.1.","........"]],  # turtle
 [["........","2......2","22.22.22","22222222",".221122.","..2222..","..2..2..","........"],  # bat
  [".2....2.",".22..22.",".222222.",".213312.",".222222.",".222222.",".222222.",".1.11.1."]],  # ghost cat
]
PAL = [((5,3,3),(27,24,16),(31,30,26)), ((8,4,2),(26,19,8),(31,28,16)), ((3,5,10),(22,27,31),(31,31,31)),
       ((2,5,2),(6,24,8),(24,31,14)), ((8,2,3),(28,12,8),(31,24,18)), ((5,2,8),(18,12,24),(28,24,31))]
def sprite(pic, frame):
    g = [[0]*8 for _ in range(16)]
    top = 8 if frame == 0 else 7
    for y, row in enumerate(pic):
        for x, ch in enumerate(row):
            g[top + y][x] = 0 if ch in ". " else int(ch)
    return g
def tile(g, r0):
    out = []
    for y in range(r0, r0 + 8):
        lo = hi = 0
        for x in range(8):
            v = g[y][x]; lo |= (v & 1) << (7 - x); hi |= (v >> 1) << (7 - x)
        out += [lo, hi]
    return out
def world(w):
    d = []
    for sp in range(2):
        for f in range(2):
            g = sprite(W[w][sp][0 if False else 0] if False else W[w][sp], f)
            d += tile(g, 0) + tile(g, 8)
    return d
if __name__ == "__main__":
    o = ["#pragma bank 29", "/* made by tools/make_life.py: do not edit */", "#include <gb/gb.h>", "#include <gb/cgb.h>",
         "const UINT8 life_tiles[6][128] = {"]
    for w in range(6):
        o.append("  {" + ",".join(str(b) for b in world(w)) + "},")
    o.append("};")
    o.append("const palette_color_t life_pal[6][4] = {")
    for p in PAL:
        o.append("  {RGB(0,0,0)," + ",".join("RGB(%d,%d,%d)" % c for c in p) + "},")
    o.append("};")
    open("src/life_art.c", "w").write("\n".join(o) + "\n")
    if len(sys.argv) > 1:
        from PIL import Image
        im = Image.new("RGB", (6*4*10, 16), (60, 80, 60))
        for w in range(6):
            for sp in range(2):
                for f in range(2):
                    g = sprite(W[w][sp], f)
                    cols = [(60,80,60)] + [tuple(c*8 for c in x) for x in PAL[w]]
                    for y in range(16):
                        for x in range(8):
                            im.putpixel(((w*4+sp*2+f)*10 + x, y), cols[g[y][x]])
        im = im.resize((im.width*4, 16*4), Image.NEAREST)
        im.save(sys.argv[1])
