#!/usr/bin/env python3
"""BUGSTAR GBC bug part art generator (step 5).

Writes src/bugparts.c + src/bugparts.h and tools/bugparts_check.png (a contact sheet to eyeball the parts).

The bug is built at run time (src/bug.c) from these parts, then cut into 8x16 sprites. Every part is a tiny
2-bit picture: 0 = clear, 1..3 = a colour slot of the palette that the region it lives in uses:

  region                   palette (colour 1, 2, 3)            what lives there
  shell, lower cells       OUTLINE, SHELL, MARKINGS/SHINE      shell, markings, legs, the tail
  wings, upper cells       WING shade, WING, WING light        wings (above the shell)
  head, lower cell         OUTLINE, HEAD, EYES                 head, eyes, mouth
  head, upper cell         OUTLINE, HORN, HAT                  antennae, horns, hats

Canvas geometry (see bug.h): shell columns are 8 px wide and 16 tall (lower: y 16..31, wings above: y 0..15);
the head column is shifted down 4 px (upper cell y 4..19, lower cell y 20..35) so hats meet the head.

Run:  python3 tools/make_bug.py
"""
import math, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LENGTHS = (16, 24, 32)                 # shell width in px for SHORT, NORMAL, LONG
SHAPES  = ("ROUND", "DOME", "FLAT", "SPIKY", "HUMPED")
EYES    = ("SMALL", "BIG", "GOOGLY", "SLEEPY", "ANGRY", "STARRY", "CYCLOPS", "SHADES")
MOUTHS  = ("NONE", "SMILE", "JAWS", "PINCERS", "FANGS", "TONGUE")
ANTS    = ("CURLED", "STRAIGHT", "DROOPY", "FEATHERY", "CLUBBED", "ZIGZAG", "WHIPS", "NONE")
HORNS   = ("NONE", "HORN", "TWINS", "RHINO", "UNICORN", "ANTLERS")
WINGS   = ("NONE", "LITTLE", "BEETLE", "FLUTTER", "BAT", "FAIRY")
TAILS   = ("NONE", "STINGER", "SPIKES", "CURLY", "LONG")
HATS    = ("NONE", "CAP", "TOP HAT", "BOW", "CROWN", "PARTY", "FLOWER", "HALO", "BEANIE", "CHEF", "HELMET", "VIKING")


# ------------------------------------------------------------------ a small pixel grid
class G:
    def __init__(s, w, h):
        s.w, s.h = w, h
        s.p = [[0] * w for _ in range(h)]
    def set(s, x, y, v):
        if 0 <= x < s.w and 0 <= y < s.h:
            s.p[y][x] = v
    def get(s, x, y):
        return s.p[y][x] if 0 <= x < s.w and 0 <= y < s.h else 0


def parse(rows, w=8):
    g = G(w, len(rows))
    for y, r in enumerate(rows):
        assert len(r) <= w, (r, w)
        for x, c in enumerate(r):
            if c in "123":
                g.set(x, y, int(c))
    return g


def outline_pass(g, inside, o=1, f=2):
    """inside: set of (x, y). Pixels with a clear 4-neighbour become outline o, the rest fill f."""
    for (x, y) in inside:
        edge = any((x + dx, y + dy) not in inside for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        g.set(x, y, o if edge else f)


# ------------------------------------------------------------------ shells (W x 16, rows 0..11 are shell, 12..15 legs)
def shell(shape, W):
    g = G(W, 16)
    cx = W / 2.0
    inside = set()
    def ell(px, py, cy, ry):
        return ((px - cx) / (W / 2.0)) ** 2 + ((py - cy) / ry) ** 2 <= 1.0
    for y in range(12):
        for x in range(W):
            px, py = x + 0.5, y + 0.5
            if shape == 0:   ok = ell(px, py, 6.5, 5.6)
            elif shape == 1: ok = py <= 12 and ell(px, py, 12.0, 12.0)
            elif shape == 2: ok = ell(px, py, 8.0, 4.2)
            elif shape == 3: ok = ell(px, py, 7.5, 4.6)
            else:
                ry = 11.6 - 4.8 * (px / W)                       # tall at the back, low at the front
                t = 1 - ((px - cx) / (W / 2.0)) ** 2
                ok = t > 0 and py <= 12 and py >= 12 - ry * math.sqrt(t)
            if ok:
                inside.add((x, y))
    if shape == 3:                                                 # spikes along the back
        top = {}
        for (x, y) in inside:
            top[x] = min(top.get(x, 99), y)
        for sx in range(3, W - 3, 5 if W > 16 else 4):
            t = top[sx]
            for k, hw in enumerate((1, 0, 0)):                     # base 3 wide, then a 1 px point
                for dx in range(-hw, hw + 1):
                    if t - 1 - k >= 0:
                        inside.add((sx + dx, t - 1 - k))
    outline_pass(g, inside)
    return g


# ------------------------------------------------------------------ wings (W x 16, drawn above the shell, bottom row touches it)
def pip(poly, px, py):
    n, c = len(poly), False
    for i in range(n):
        x1, y1 = poly[i]; x2, y2 = poly[(i + 1) % n]
        if (y1 > py) != (y2 > py) and px < (x2 - x1) * (py - y1) / (y2 - y1) + x1:
            c = not c
    return c


def blob(inside, cx, cy, rx, ry, shear=0.0):
    for y in range(16):
        for x in range(inside_w[0]):
            px, py = x + 0.5 + shear * (15.5 - (y + 0.5)) * -1, y + 0.5   # shear: the top leans back (left)
            if ((px - cx) / rx) ** 2 + ((py - cy) / ry) ** 2 <= 1.0:
                inside.add((x, y))


inside_w = [24]


def vein(g, x0, y0, x1, y1):
    n = max(abs(x1 - x0), abs(y1 - y0), 1)
    for i in range(n + 1):
        x = round(x0 + (x1 - x0) * i / n); y = round(y0 + (y1 - y0) * i / n)
        if g.get(x, y) == 2:
            g.set(x, y, 1)


def wings(kind, W):
    g = G(W, 16)
    if kind == 0:
        return g
    inside_w[0] = W
    ins = set()
    rx0 = W * 0.55                                                 # where the wings join the back
    if kind == 1:                                                  # little: two small buds
        blob(ins, rx0 - 3, 12.5, 4.2, 3.4, 0.35); blob(ins, rx0 + 1, 13.2, 3.2, 2.6, 0.2)
    elif kind == 2:                                                # beetle: a lifted hard case and the soft wing under it
        blob(ins, W * 0.46, 11.5, W * 0.30, 4.2, 0.30); blob(ins, W * 0.40, 14.0, W * 0.20, 2.0, 0.0)
    elif kind == 3:                                                # butterfly: big upper wing and a smaller lower one
        blob(ins, W * 0.42, 7.6, W * 0.25, 7.4, 0.22); blob(ins, W * 0.34, 12.8, 6.0, 3.1, 0.1)
    elif kind == 4:                                                # bat: bony fingers, scalloped edge
        pts = [(1, 0), (0, -6), (-3, -10), (-7, -9), (-12, -7), (-10, -4), (-8, -5), (-6, -2), (-4, -4), (-2, -1), (0, 0)]
        k = W / 24.0
        poly = [(rx0 + dx * k, 15.5 + dy) for dx, dy in pts]
        for y in range(16):
            for x in range(W):
                if pip(poly, x + 0.5, y + 0.5):
                    ins.add((x, y))
    else:                                                          # fairy: two tall slim petals
        blob(ins, rx0 - 3, 8.0, 2.6, 7.4, 0.40); blob(ins, rx0 - 6, 10.5, 2.2, 5.0, 0.35)
    outline_pass(g, ins, 1, 2)
    if kind == 1:
        pass
    elif kind == 2:
        vein(g, int(W * 0.46), 14, int(W * 0.30), 9)
    elif kind == 3:
        vein(g, int(W * 0.36), 15, int(W * 0.46), 4)
    elif kind == 4:
        for dx, dy in ((-3, -9), (-7, -8), (-10, -6)):
            vein(g, int(rx0 - 1), 15, int(rx0 + dx * W / 24.0), 15 + dy)
    else:
        vein(g, int(rx0 - 3), 15, int(rx0 - 6), 2)
    # a soft highlight crescent on the upper-left of anything big enough
    for (x, y) in sorted(ins):
        if g.get(x, y) == 2 and g.get(x - 1, y) == 1 and g.get(x, y - 1) in (1, 0) and kind in (2, 3, 4, 5):
            g.set(x, y, 3)
    low = max(y for y in range(16) for x in range(W) if g.p[y][x])      # rest the wings on the shell
    if low < 15:
        sh = 15 - low
        for y in range(15, sh - 1, -1):
            g.p[y] = g.p[y - sh][:]
        for y in range(sh):
            g.p[y] = [0] * W
    return g


# ------------------------------------------------------------------ head + head-column parts
def head():
    g = G(8, 16)
    ins = set()
    for y in range(10):
        for x in range(8):
            dx, dy = (x + 0.5 - 4.0) / 4.4, (y + 0.5 - 5.0) / 5.0
            if abs(dx) ** 2.0 + abs(dy) ** 2.0 <= 1.0:
                ins.add((x, y))
    outline_pass(g, ins)
    return g


# eyes / mouths: (ox, oy, rows) placed on the head's lower cell. 3 = eye colour, 1 = pupil / line
EYE_ART = [
    (4, 3, ["31", "31"]),                                                  # SMALL
    (3, 2, [".33", "331", "331", ".33"]),                                  # BIG
    (3, 2, [".33.", "3311", "3311", ".33."]),                              # GOOGLY
    (3, 3, ["111", "331", "331"]),                                         # SLEEPY
    (3, 2, ["1..", ".11", "331", "331"]),                                  # ANGRY
    (3, 2, [".3.", "333", "313", ".3."]),                                  # STARRY
    (2, 2, [".33.", "3333", "3113", "3113", ".33."]),                      # CYCLOPS
    (2, 3, ["11111", "11111", ".3..."]),                                   # SHADES
]
MOUTH_ART = [
    None,
    (3, 6, ["1..1", ".11."]),                                              # SMILE
    (5, 5, ["..1", "..1", ".11", ".1."]),                                  # JAWS (mandibles)
    (4, 6, ["1.1", ".1.", "1.1"]),                                         # PINCERS
    (3, 6, ["1111", "3.3."]),                                              # FANGS
    (3, 6, ["111.", ".33.", ".33."]),                                      # TONGUE
]

# head-upper art: anchored to the bottom of the cell (the last row sits on the head). 1 line, 2 horn, 3 hat
ANT_ART = [
    ["....1...", "...1....", "...1....", "..1.....", "..1.....", "..1....."],                       # CURLED (tweaked below)
    ["......1.", ".....1..", "....1.1.", "...1..1.", "..1....."],                                   # STRAIGHT
    ["...11...", "..1..1..", "..1...1.", "..1.....", ".1......"],                                   # DROOPY
    ["....1.1.", ".....1..", "....11..", "...1.1..", "...1....", "..1....."],                       # FEATHERY
    ["......11", "......11", ".....1..", "....1...", "...1....", "..1....."],                       # CLUBBED
    ["....1...", ".....1..", "....1...", ".....1..", "....1...", "...1...."],                       # ZIGZAG
    ["...111..", "..1...1.", ".1....1.", ".1......", "1.......", "1....1..", "..1.1..."],             # WHIPS
]
HORN_ART = [
    None,
    ["....1...", "...121..", "...121..", "..1221.."],                                              # HORN
    ["..1..1..", ".121.121", ".121.121", "12211221"],                                              # TWINS (two on the head)
    ["......1.", ".....121", "....1221", "....1221"],                                              # RHINO (nose)
    ["......1.", "......1.", ".....121", ".....121", "....1221", "....1221", "...12221"],            # UNICORN
    ["1.1.1.1.", "1.1.1.1.", "121.121.", ".1212...", "..121...", "..121..."],                       # ANTLERS
]
HAT_ART = [
    None,
    [".11111..", "1333331.", "13333311"],                                                          # CAP
    ["..1111..", "..1331..", "..1331..", "..1331..", ".111111.", "13333331"],                         # TOP HAT
    [".11..11.", "13311331", "13311331", ".11..11."],                                              # BOW
    ["1.1.1.1.", "1313131.", "13333331", "11111111"],                                              # CROWN
    ["...1....", "..131...", "..131...", ".13331..", ".13331..", "13333331"],                       # PARTY
    ["..232...", ".23132..", "..232...", "...1...."],                                              # FLOWER (petals 2/3, centre)
    ["..3333..", ".3....3.", "..3333.."],                                                          # HALO
    ["....2...", "...121..", "..13331.", ".1333331", "13333331"],                                   # BEANIE
    [".1.11.1.", "13133131", "13333331", ".133331.", ".133331."],                                   # CHEF
    ["..1111..", ".133331.", "13131331", "13333331"],                                              # HELMET
    ["2......2", "12.11.21", "1213312.", "13333331"],                                              # VIKING
]


def pad(rows, w=8):
    return [r.ljust(w, ".")[:w] for r in rows]


# ------------------------------------------------------------------ tails (8 x 16, lower cell left of the shell)
TAIL_ART = [
    None,
    (0, 5, ["1.......", "21......", ".211....", "..2221..", "...2221.", "....2221"]),               # STINGER
    (0, 3, ["....1...", "...121.1", "..1221..", ".1222111", "12222221"]),                          # SPIKES
    (0, 2, [".111....", "1..1....", "1.1.....", "1..1.11.", ".11...11", "......1."]),                 # CURLY
    (0, 7, ["........", "1.......", "11......", ".11111..", "...1111."]),                            # LONG
]


# ------------------------------------------------------------------ pack + emit
def pack(g):
    out = []
    flat = [g.p[y][x] for y in range(g.h) for x in range(g.w)]
    while len(flat) % 4:
        flat.append(0)
    for i in range(0, len(flat), 4):
        out.append(flat[i] | flat[i + 1] << 2 | flat[i + 2] << 4 | flat[i + 3] << 6)
    return out


def trim(g):
    """crop to the bounding box -> (ox, oy, grid) so empty parts cost nothing"""
    xs = [x for y in range(g.h) for x in range(g.w) if g.p[y][x]]
    ys = [y for y in range(g.h) for x in range(g.w) if g.p[y][x]]
    if not xs:
        return 0, 0, G(0, 0)
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    t = G(x1 - x0 + 1, y1 - y0 + 1)
    for y in range(t.h):
        for x in range(t.w):
            t.p[y][x] = g.p[y0 + y][x0 + x]
    return x0, y0, t


class Out:
    def __init__(s):
        s.data, s.parts, s.n = [], {}, 0
    def add(s, name, ox, oy, g):
        raw = pack(g) if g.w else []
        if raw:
            s.data.append("static const unsigned char d%d[] = {%s};" % (s.n, ",".join(str(b) for b in raw)))
            ref = "d%d" % s.n
        else:
            ref = "0"
        s.n += 1
        s.parts.setdefault(name, []).append("{%d,%d,%d,%d,%s}" % (g.w, g.h, ox, oy, ref))


def place(rows, ox, oy):
    """rows of art -> an 8 x 16 cell with the art at (ox, oy)"""
    g = parse(pad(rows, 8 - ox), 8)
    gg = G(8, 16)
    for y in range(g.h):
        for x in range(g.w):
            gg.set(x + ox, y + oy, g.p[y][x])
    return gg


def anchored(rows):
    """rows of art sitting on the bottom of an 8 x 16 cell (hats, horns, antennae)"""
    g = parse(pad(rows, 8), 8)
    gg = G(8, 16)
    for y in range(g.h):
        for x in range(8):
            gg.set(x, 16 - g.h + y, g.p[y][x])
    return gg


def main():
    o = Out()
    def add(name, g):
        ox, oy, t = trim(g)
        o.add(name, ox, oy, t)
    for sh in range(len(SHAPES)):                    # bp_shell[shape * 3 + length]
        for W in LENGTHS:
            o.add("bp_shell", 0, 0, shell(sh, W))
    for k in range(len(WINGS)):                      # bp_wings[kind * 3 + length]
        for W in LENGTHS:
            add("bp_wings", wings(k, W))
    add("bp_head", head())
    for ox, oy, rows in EYE_ART:
        add("bp_eye", place(rows, ox, oy))
    add("bp_mouth", G(8, 16))
    for ox, oy, rows in MOUTH_ART[1:]:
        add("bp_mouth", place(rows, ox, oy))
    for rows in ANT_ART:
        add("bp_ant", anchored(rows))
    add("bp_ant", G(8, 16))                          # NONE
    add("bp_horn", G(8, 16))
    for rows in HORN_ART[1:]:
        add("bp_horn", anchored(rows))
    add("bp_hat", G(8, 16))
    for rows in HAT_ART[1:]:
        add("bp_hat", anchored(rows))
    add("bp_tail", G(8, 16))
    for ox, oy, rows in TAIL_ART[1:]:
        add("bp_tail", place(rows, ox, oy))
    dump_sheet()
    write_c(o)


def write_c(o):
    c = ['/* GENERATED by tools/make_bug.py - do not edit. Bug part art, 2 bits per pixel (see bug.h for BugPart). */',
         '#include "bug.h"']
    c += o.data
    for name, items in o.parts.items():
        c.append("const BugPart %s[%d] = {%s};" % (name, len(items), ",".join(items)))
    open(os.path.join(ROOT, "src", "bugparts.c"), "w").write("\n".join(c) + "\n")
    print("wrote src/bugparts.c: %d pictures" % o.n)


# ------------------------------------------------------------------ contact sheet
def dump_sheet():
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return
    S = 6
    pal = {
        "body": [(0, 0, 0), (16, 16, 36), (92, 108, 148), (230, 190, 70)],
        "head": [(0, 0, 0), (16, 16, 36), (120, 140, 190), (250, 250, 250)],
        "acc":  [(0, 0, 0), (16, 16, 36), (210, 170, 60), (210, 60, 70)],
        "wing": [(0, 0, 0), (60, 90, 140), (190, 225, 255), (245, 250, 255)],
    }
    im = Image.new("RGB", (1500, 1000), (150, 160, 175))
    d = ImageDraw.Draw(im)
    def blit(g, x0, y0, p):
        for y in range(g.h):
            for x in range(g.w):
                v = g.p[y][x]
                if v:
                    d.rectangle([x0 + x * S, y0 + y * S, x0 + x * S + S - 1, y0 + y * S + S - 1], fill=pal[p][v])
    def label(x, y, t):
        d.text((x, y), t, fill=(0, 0, 0))
    y = 6
    label(10, y, "shells (shape x length); legs are added at run time under rows 12..15"); y += 14
    for sh in range(5):
        x = 10
        for W in LENGTHS:
            blit(shell(sh, W), x, y, "body"); x += W * S + 16
        y += 16 * S + 4
    # wings with a round shell under them
    x = 520
    yy = 20
    label(x, yy - 14, "wings over a NORMAL round shell")
    for k in range(1, 6):
        blit(wings(k, 24), x, yy, "wing"); blit(shell(0, 24), x, yy + 16 * S, "body")
        x += 24 * S + 10
        if k == 3:
            x = 520; yy += 32 * S + 14
    y = max(y, yy + 32 * S) + 14
    hd = head()
    label(10, y, "antennae A, horns H, hats T: upper cell above the head (head is the lower cell)"); y += 14
    items = [("A%d" % i, anchored(r)) for i, r in enumerate(ANT_ART)]
    items += [("H%d" % i, anchored(r)) for i, r in enumerate(HORN_ART) if r]
    items += [("T%d" % i, anchored(r)) for i, r in enumerate(HAT_ART) if r]
    x = 10
    for name, g in items:
        blit(g, x, y, "acc"); blit(hd, x, y + 16 * S, "head"); label(x, y + 26 * S + 4, name)
        x += 8 * S + 12
    y += 26 * S + 22
    label(10, y, "eyes, mouths (on the head) and tails"); y += 14
    x = 10
    for ox, oy, rows in EYE_ART + MOUTH_ART[1:]:
        blit(hd, x, y, "head"); blit(place(rows, ox, oy), x, y, "head"); x += 8 * S + 12
    for ox, oy, rows in TAIL_ART[1:]:
        blit(place(rows, ox, oy), x, y, "body"); x += 8 * S + 12
    im.crop((0, 0, 1500, y + 16 * S + 12)).save(os.path.join(ROOT, "tools", "bugparts_check.png"))


if __name__ == "__main__":
    main()
