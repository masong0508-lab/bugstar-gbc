#!/usr/bin/env python3
"""BUGSTAR GBC font + UI tile generator.

Writes src/font.c (128 tiles, 2bpp GB format) and src/font.h (tile numbers + string macros),
and tools/font_check.png (specimen sheet + mock screen) so you can eyeball the result.

Layout: tile number == ASCII code, so a C string IS a row of tile numbers (no translation).
  0         blank
  1..8      box frame pieces        10..15 cursor, arrows, heart, coin
  16..24    bar fill 0/8..8/8       25,26  bar end caps      27,28 checkbox  29,30 bullet, star
  32..96    space ! " # ... Z ] ^ _ (hand drawn 5x6 caps/digits/punctuation)
  97..122   a..z = copies of A..Z (the game is all caps; lowercase shows as caps, never as gaps)
  96,123..126  ` { | } ~          127 solid block
Glyph ink is colour 3, the soft drop shadow is colour 1, frame/bars use colours 1..3 directly.
Run:  python3 tools/make_font.py            (add --no-shadow for a flat font)
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHADOW = '--no-shadow' not in sys.argv

# ---- 5x6 glyphs (drawn into tile rows 1..6, cols 1..5) -------------------------------------------
GLYPHS = {
 ' ': ".....|.....|.....|.....|.....|.....",
 '!': "..#..|..#..|..#..|..#..|.....|..#..",
 '"': ".#.#.|.#.#.|.....|.....|.....|.....",
 '#': ".#.#.|#####|.#.#.|#####|.#.#.|.....",
 '$': "..#..|.####|#.#..|.###.|..#.#|####.",
 '%': "##..#|##.#.|...#.|..#..|.#.##|#..##",
 '&': ".##..|#..#.|.##..|#.#.#|#..#.|.##.#",
 "'": "..#..|..#..|.....|.....|.....|.....",
 '(': "...#.|..#..|..#..|..#..|..#..|...#.",
 ')': ".#...|..#..|..#..|..#..|..#..|.#...",
 '*': ".....|#.#.#|.###.|#.#.#|.....|.....",
 '+': ".....|.....|..#..|#####|..#..|.....",
 ',': ".....|.....|.....|.....|..#..|.#...",
 '-': ".....|.....|.....|#####|.....|.....",
 '.': ".....|.....|.....|.....|.....|..#..",
 '/': "....#|...#.|..#..|..#..|.#...|#....",
 '0': ".###.|#..##|#.#.#|##..#|#...#|.###.",
 '1': "..#..|.##..|..#..|..#..|..#..|.###.",
 '2': ".###.|#...#|...#.|..#..|.#...|#####",
 '3': "####.|....#|.###.|....#|#...#|.###.",
 '4': "...#.|..##.|.#.#.|#..#.|#####|...#.",
 '5': "#####|#....|####.|....#|#...#|.###.",
 '6': ".###.|#....|####.|#...#|#...#|.###.",
 '7': "#####|....#|...#.|..#..|.#...|.#...",
 '8': ".###.|#...#|.###.|#...#|#...#|.###.",
 '9': ".###.|#...#|#...#|.####|....#|.###.",
 ':': ".....|..#..|.....|.....|..#..|.....",
 ';': ".....|..#..|.....|.....|..#..|.#...",
 '<': "...#.|..#..|.#...|..#..|...#.|.....",
 '=': ".....|#####|.....|#####|.....|.....",
 '>': ".#...|..#..|...#.|..#..|.#...|.....",
 '?': ".###.|#...#|...#.|..#..|.....|..#..",
 '@': ".###.|#...#|#.###|#.###|#....|.###.",
 'A': ".###.|#...#|#...#|#####|#...#|#...#",
 'B': "####.|#...#|####.|#...#|#...#|####.",
 'C': ".####|#....|#....|#....|#....|.####",
 'D': "####.|#...#|#...#|#...#|#...#|####.",
 'E': "#####|#....|####.|#....|#....|#####",
 'F': "#####|#....|####.|#....|#....|#....",
 'G': ".####|#....|#.###|#...#|#...#|.###.",
 'H': "#...#|#...#|#####|#...#|#...#|#...#",
 'I': ".###.|..#..|..#..|..#..|..#..|.###.",
 'J': "..###|...#.|...#.|...#.|#..#.|.##..",
 'K': "#...#|#..#.|###..|#..#.|#...#|#...#",
 'L': "#....|#....|#....|#....|#....|#####",
 'M': "#...#|##.##|#.#.#|#...#|#...#|#...#",
 'N': "#...#|##..#|#.#.#|#..##|#...#|#...#",
 'O': ".###.|#...#|#...#|#...#|#...#|.###.",
 'P': "####.|#...#|####.|#....|#....|#....",
 'Q': ".###.|#...#|#...#|#.#.#|#..#.|.##.#",
 'R': "####.|#...#|####.|#.#..|#..#.|#...#",
 'S': ".####|#....|.###.|....#|....#|####.",
 'T': "#####|..#..|..#..|..#..|..#..|..#..",
 'U': "#...#|#...#|#...#|#...#|#...#|.###.",
 'V': "#...#|#...#|#...#|#...#|.#.#.|..#..",
 'W': "#...#|#...#|#...#|#.#.#|##.##|#...#",
 'X': "#...#|.#.#.|..#..|.#.#.|#...#|#...#",
 'Y': "#...#|#...#|.#.#.|..#..|..#..|..#..",
 'Z': "#####|....#|...#.|..#..|.#...|#####",
 '[': ".###.|.#...|.#...|.#...|.#...|.###.",
 '\\': "#....|.#...|..#..|..#..|...#.|....#",
 ']': ".###.|...#.|...#.|...#.|...#.|.###.",
 '^': "..#..|.#.#.|#...#|.....|.....|.....",
 '_': ".....|.....|.....|.....|.....|#####",
 '`': ".#...|..#..|.....|.....|.....|.....",
 '{': "..##.|..#..|.#...|..#..|..#..|..##.",
 '|': "..#..|..#..|..#..|..#..|..#..|..#..",
 '}': ".##..|..#..|...#.|..#..|..#..|.##..",
 '~': ".....|.##.#|#.##.|.....|.....|.....",
}

# ---- hand-coloured 8x8 UI tiles (digits are colours 1..3, '.' is 0) ------------------------------
def T(*rows):
    assert len(rows) == 8 and all(len(r) == 8 for r in rows), rows
    return list(rows)

SPECIAL = {   # tile number -> (macro name, rows)
 1:  ("BOX_TL", T("........","..333333",".3222222",".32.....",".32.....",".32.....",".32.....",".32.....")),
 2:  ("BOX_T",  T("........","33333333","22222222","........","........","........","........","........")),
 3:  ("BOX_TR", T("........","333333..","2222223.",".....23.",".....23.",".....23.",".....23.",".....23.")),
 4:  ("BOX_L",  T(".32.....",".32.....",".32.....",".32.....",".32.....",".32.....",".32.....",".32.....")),
 5:  ("BOX_R",  T(".....23.",".....23.",".....23.",".....23.",".....23.",".....23.",".....23.",".....23.")),
 6:  ("BOX_BL", T(".32.....",".32.....",".32.....",".32.....",".32.....",".3222222","..333333","........")),
 7:  ("BOX_B",  T("........","........","........","........","........","22222222","33333333","........")),
 8:  ("BOX_BR", T(".....23.",".....23.",".....23.",".....23.",".....23.","2222223.","333333..","........")),
 10: ("CURSOR", T("........","..3.....","..33....","..333...","..333...","..33....","..3.....","........")),
 11: ("ARROW_DN", T("........","........",".333333.","..3333..","...33...","........","........","........")),
 12: ("ARROW_UP", T("........","........","........","...33...","..3333..",".333333.","........","........")),
 13: ("ARROW_L", T("........",".....3..","....33..","...333..","...333..","....33..",".....3..","........")),
 14: ("HEART", T("........",".33.33..","3333333.","3333333.",".33333..","..333...","...3....","........")),
 15: ("COIN",  T("........","..3333..",".322223.",".322223.",".322223.",".322223.","..3333..","........")),
 25: ("BAR_L", T("........","..333333","..311111","..311111","..311111","..311111","..333333","........")),
 26: ("BAR_R", T("........","333333..","111113..","111113..","111113..","111113..","333333..","........")),
 27: ("CHK_OFF", T("........",".333333.",".3....3.",".3....3.",".3....3.",".3....3.",".333333.","........")),
 28: ("CHK_ON",  T("........",".333333.",".322223.",".322223.",".322223.",".322223.",".333333.","........")),
 29: ("BULLET", T("........","........","...33...","..3333..","..3333..","...33...","........","........")),
 30: ("STAR",  T("........","...3....","...3....","3333333.","..333...","..3.3...","........","........")),
 127: ("BLOCK", T("........",".333333.",".333333.",".333333.",".333333.",".333333.",".333333.","........")),
}

def bar_tile(n):                       # n of 8 pixels filled (colour 2), rest colour 1, frame colour 3
    inner = "2" * n + "1" * (8 - n)
    return T("........", "33333333", inner, inner, inner, inner, "33333333", "........")

def glyph_tile(rows):
    t = [[0] * 8 for _ in range(8)]
    ink = [(r + 1, c + 1) for r, row in enumerate(rows) for c, ch in enumerate(row) if ch == '#']
    if SHADOW:
        for (y, x) in ink:
            if y + 1 < 8 and x + 1 < 8: t[y + 1][x + 1] = 1
    for (y, x) in ink: t[y][x] = 3
    return t

def parse(rows):
    return [[0 if ch == '.' else int(ch) for ch in r] for r in rows]

def build():
    tiles = {i: [[0] * 8 for _ in range(8)] for i in range(128)}
    for ch, spec in GLYPHS.items():
        rows = spec.split('|')
        assert len(rows) == 6 and all(len(r) == 5 for r in rows), (ch, rows)
        tiles[ord(ch)] = glyph_tile(rows)
    for i in range(26):                                   # a..z look like A..Z
        tiles[97 + i] = [r[:] for r in tiles[65 + i]]
    for n, (_, rows) in SPECIAL.items():
        tiles[n] = parse(rows)
    for n in range(9):
        tiles[16 + n] = parse(bar_tile(n))
    return tiles

def pack(t):
    out = []
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            c = t[y][x]
            lo |= (c & 1) << (7 - x); hi |= ((c >> 1) & 1) << (7 - x)
        out += [lo, hi]
    return out

def octal(n): return "\\%03o" % n

def write_c(tiles):
    names = {n: nm for n, (nm, _) in SPECIAL.items()}
    h = ["/* GENERATED by tools/make_font.py - do not edit. Tile number == ASCII code. */",
         "#ifndef FONT_H", "#define FONT_H",
         "#define FONT_NTILES 128", "extern const unsigned char bs_font[FONT_NTILES * 16];",
         "/* tile numbers */"]
    for n, nm in sorted(names.items()): h.append("#define T_%-9s %d" % (nm, n))
    h.append("#define T_BLANK     0")
    for n in range(9): h.append("#define T_BAR%d      %d" % (n, 16 + n))
    h.append("/* the same tiles as 1-char string literals, to embed in text: \"HP \" S_HEART */")
    for n, nm in sorted(names.items()): h.append('#define S_%-9s "%s"' % (nm, octal(n)))
    h += ["#endif", ""]
    open(os.path.join(ROOT, "src", "font.h"), "w").write("\n".join(h))
    c = ["/* GENERATED by tools/make_font.py - do not edit. 128 tiles x 16 bytes, GB 2bpp. */",
         '#include "font.h"', "const unsigned char bs_font[FONT_NTILES * 16] = {"]
    for i in range(128):
        label = chr(i) if (chr(i).isalnum()) else names.get(i, "")
        c.append("/*%3d %-8s*/ %s," % (i, label, ",".join("0x%02X" % b for b in pack(tiles[i]))))
    c += ["};", ""]
    open(os.path.join(ROOT, "src", "font.c"), "w").write("\n".join(c))

# ---- specimen PNG ---------------------------------------------------------------------------------
def write_png(tiles):
    try:
        from PIL import Image
    except ImportError:
        print("(no Pillow: skipped tools/font_check.png)"); return
    rgb5 = lambda r, g, b: (r * 255 // 31, g * 255 // 31, b * 255 // 31)
    pal = [rgb5(1, 1, 6), rgb5(26, 6, 6), rgb5(30, 24, 4), rgb5(31, 31, 31)]   # = text palette 0 in src/text.c
    def blit(img, t, px, py, s):
        for y in range(8):
            for x in range(8):
                col = pal[t[y][x]]
                for dy in range(s):
                    for dx in range(s): img.putpixel((px + x * s + dx, py + y * s + dy), col)
    S = 3
    W = max(160 * S, 16 * 8 * S + 20)
    img = Image.new("RGB", (W, 144 * S + 8 * 8 * S + 30), (40, 40, 40))
    scr = Image.new("RGB", (160, 144), pal[0])
    def tile(x, y, n): blit(scr, tiles[n], x * 8, y * 8, 1)
    def text(x, y, s):
        for i, ch in enumerate(s): tile(x + i, y, ord(ch) & 127)
    def box(x, y, w, h):
        for r in range(h):
            for c in range(w):
                n = 0
                if r == 0: n = 1 if c == 0 else 3 if c == w - 1 else 2
                elif r == h - 1: n = 6 if c == 0 else 8 if c == w - 1 else 7
                elif c == 0: n = 4
                elif c == w - 1: n = 5
                tile(x + c, y + r, n)
    def bar(x, y, w, v, m):
        tile(x, y, 25); px = (w - 2) * 8; f = v * px // m
        for i in range(w - 2): tile(x + 1 + i, y, 16 + max(0, min(8, f - i * 8)))
        tile(x + w - 1, y, 26)
    text(1, 0, "BUGSTAR TEXT ENGINE"); text(1, 1, "THE QUICK BROWN FOX")
    text(1, 2, "JUMPS OVER 0123456789"[:18])
    text(1, 3, "!?.,:;'-+*/%&#@$()=<>"[:18])
    text(1, 4, "lower = small caps"[:18])
    box(0, 6, 20, 5)
    text(2, 7, "HELLO, BUG!"); text(2, 8, "PRESS "); tile(8, 8, 28); tile(9, 8, 27)
    tile(18, 9, 11)
    tile(1, 12, 14); text(2, 12, "HP"); bar(5, 12, 10, 70, 100); tile(1, 13, 15); text(2, 13, "GOLD 1234")
    tile(1, 15, 10); text(2, 15, "CURSOR"); tile(10, 15, 29); tile(11, 15, 30); tile(12, 15, 12); tile(13, 15, 13)
    img.paste(scr.resize((160 * S, 144 * S), Image.NEAREST), (0, 0))
    for n in range(128): blit(img, tiles[n], 10 + (n % 16) * 8 * S, 144 * S + 10 + (n // 16) * 8 * S, S)
    img.save(os.path.join(ROOT, "tools", "font_check.png"))

if __name__ == "__main__":
    tl = build()
    write_c(tl); write_png(tl)
    print("wrote src/font.c, src/font.h, tools/font_check.png (shadow %s)" % ("on" if SHADOW else "off"))
