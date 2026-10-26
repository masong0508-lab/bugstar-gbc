#!/usr/bin/env python3
"""Makes src/kart_art.h: the road, sky and mountain tiles/maps and the sprite art for KART WORLD (src/kart.c, bank 24).
Run: python3 tools/make_kart.py"""
import math

def c15(r, g, b): return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)

# ---------------------------------------------------------------- background: sky, mountains, the road in perspective
HZ = 64                      # first road scanline
NR = 80                      # road scanlines (64..143)
def hw(i): return 3.0 + i * 72.0 / 79.0

def mount(x):                # mountain height (pixels) at screen x; the pattern repeats every 256 pixels so the map wraps
    h = 11 + 8 * math.sin(2 * math.pi * x / 128.0) + 4 * math.sin(2 * math.pi * x / 64.0 + 1.3) + 2 * math.sin(2 * math.pi * x / 32.0 + 0.4)
    return max(3, min(24, int(round(h))))

def sky_px(y, x):
    if y < 32: return 0
    if y < 40: return 1
    h = mount(x)
    top = HZ - h
    if y < top: return 1
    return 3 if y < top + 2 else 2

def road_px(y, x):
    i = y - HZ
    w = hw(i); kw = max(1.0, w * 0.13); lw = max(0.5, w * 0.035)
    d = abs(x + 0.5 - 80)
    if d > w: return 0
    if d > w - kw: return 1
    if d <= lw: return 3
    return 2

tiles = []; tindex = {}
def tile(px):                # px: 8 rows of 8 values
    key = tuple(tuple(r) for r in px)
    if key not in tindex:
        tindex[key] = len(tiles); tiles.append(key)
    return tindex[key]

def enc(t):
    out = []
    for r in t:
        lo = hi = 0
        for x, v in enumerate(r):
            lo |= (v & 1) << (7 - x); hi |= ((v >> 1) & 1) << (7 - x)
        out += [lo, hi]
    return out

band = [0, 1, 2, 3, 4, 4, 5, 5, 6, 6]         # band number of each road tile row (8..17): thin far bands, thick near ones
ROWS = list(range(2, 18))
mp = []; at = []
for ty in ROWS:
    mrow = []; arow = []
    for tx in range(32):
        px = []
        for yy in range(8):
            y = ty * 8 + yy
            px.append([(sky_px(y, tx * 8 + xx) if ty < 8 else road_px(y, tx * 8 + xx)) for xx in range(8)])
        mrow.append(tile(px))
        if ty < 8: arow.append(0x08 | 6)
        else: arow.append(0x08 | (4 + (band[ty - 8] & 1)))
    mp.append(mrow); at.append(arow)
assert len(tiles) <= 256, len(tiles)

pal = [c15(70, 175, 70), c15(225, 50, 50), c15(115, 115, 125), c15(240, 240, 240),      # 4: light band
       c15(45, 145, 55), c15(240, 240, 240), c15(100, 100, 110), c15(100, 100, 110),     # 5: dark band
       c15(70, 130, 240), c15(150, 200, 255), c15(95, 105, 160), c15(215, 225, 245)]    # 6: sky

hwt = [int(round(hw(i))) for i in range(NR)]
qa = [int(round(((NR - 1 - i) ** 2) * 64 / ((NR - 1) ** 2))) for i in range(NR)]
A = 14.0
ytab = [min(143, HZ + int(79 * A / (A + dz))) for dz in range(96)]

# ---------------------------------------------------------------- sprites: 8x16 units
units = []                   # each: 32 bytes (16 rows of 8 pixels -> 2 tiles)
def canvas(w, h): return [[0] * w for _ in range(h)]
def rect(c, x0, y0, x1, y1, v):
    for y in range(int(y0), int(y1)):
        for x in range(int(x0), int(x1)):
            if 0 <= y < len(c) and 0 <= x < len(c[0]): c[y][x] = v
def ell(c, cx, cy, rx, ry, v):
    for y in range(len(c)):
        for x in range(len(c[0])):
            if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0: c[y][x] = v
def put(cv, w, h, src):      # src content drawn bottom-aligned and centred onto the canvas
    sh = len(src); sw = len(src[0]); ox = (w - sw) // 2; oy = h - sh
    for y in range(sh):
        for x in range(sw):
            if src[y][x]: cv[oy + y][ox + x] = src[y][x]
def tree(w, h):
    c = canvas(w, h); th = max(2, h // 4); tw = max(1, w // 5)
    rect(c, (w - tw) // 2, h - th, (w + tw + 1) // 2 if tw > 1 else (w - tw) // 2 + 1, h, 1)
    ch = h - th + 1
    ell(c, w / 2.0, ch / 2.0, w / 2.0, ch / 2.0, 2)
    ell(c, w / 2.0 - w * 0.12, ch / 2.0 - ch * 0.15, w * 0.28, ch * 0.28, 3)
    return c
def rock(w, h):
    c = canvas(w, h)
    for y in range(h):
        for x in range(w):
            d = ((x + 0.5 - w / 2.0) / (w / 2.0)) ** 2 + ((y + 0.5 - (h - 0.5)) / (h * 0.95)) ** 2
            if d <= 1.0: c[y][x] = 2 if (x < w * 0.7 and y > h * 0.25) else 1
    ell(c, w * 0.38, h * 0.45, w * 0.18, h * 0.16, 3)
    for x in range(w): c[h - 1][x] = 1 if c[h - 1][x] else 0
    return c
def rear(w, h, sh=0):
    c = canvas(w, h)
    ww = max(2, w // 5); wh = max(2, int(h * 0.5))
    rect(c, 0, h - wh, ww, h, 1); rect(c, w - ww, h - wh, w, h, 1)
    rect(c, ww // 2, h * 0.3, w - ww // 2, h - max(1, h * 0.1), 2)
    rect(c, ww // 2, h - max(1, h * 0.1) - 1, w - ww // 2, h - max(1, h * 0.1), 1)
    rect(c, w * 0.2, 0, w * 0.8, max(1, h // 7), 1)
    rect(c, w * 0.42, h * 0.55, w * 0.58, h * 0.8, 3)
    ell(c, w / 2.0, h * 0.34, max(1.6, w / 6.0), max(1.6, h * 0.2), 3)
    if sh:                                                    # steering: the upper half leans
        for y in range(int(h * 0.6)):
            row = c[y][:]; c[y] = [0] * w
            for x in range(w):
                if 0 <= x + sh < w: c[y][x + sh] = row[x]
    return c
def slice_units(c):          # canvas (w multiple of 8, h multiple of 16) -> units, column-major (col0 top..bottom, col1 ...)
    w = len(c[0]); h = len(c); out = []
    for cx in range(w // 8):
        for cy in range(h // 16):
            u = []
            for yy in range(16):
                u.append([c[cy * 16 + yy][cx * 8 + xx] for xx in range(8)])
            out.append(u)
    return out
def add(c):
    base = len(units)
    for u in slice_units(c): units.append(u)
    return base
def sized(fn, cw, ch, w, h, **kw):
    cv = canvas(cw, ch); put(cv, cw, ch, fn(w, h, **kw) if kw else fn(w, h)); return cv

U = {}
for nm, fn in (('tree', tree), ('rock', rock)):
    U[nm + '0'] = add(sized(fn, 8, 16, 7, 12))
    U[nm + '1'] = add(sized(fn, 16, 16, 14, 16))
    U[nm + '2'] = add(sized(fn, 16, 32, 14, 30))
U['ai0'] = add(sized(rear, 8, 16, 8, 8))
U['ai1'] = add(sized(rear, 16, 16, 16, 11))
U['ai2'] = add(sized(rear, 24, 16, 24, 16))
for k, sh in enumerate((-2, 0, 2)): U['me%d' % k] = add(sized(rear, 24, 16, 24, 16, sh=sh))

def itembox(w, h):                                       # a golden "?" cube
    c = canvas(w, h)
    rect(c, 0, 0, w, h, 1); rect(c, 1, 1, w - 1, h - 1, 2)
    if w >= 12:
        q = max(1, w // 6); mx = w // 2
        rect(c, mx - q * 1.5, h * 0.2, mx + q * 1.5, h * 0.2 + q, 3); rect(c, mx + q * 0.5, h * 0.2 + q, mx + q * 1.5, h * 0.2 + q * 2.2, 3)
        rect(c, mx - q * 0.5, h * 0.2 + q * 2.2, mx + q * 0.5, h * 0.2 + q * 3.2, 3); rect(c, mx - q * 0.5, h * 0.2 + q * 4.0, mx + q * 0.5, h * 0.2 + q * 5.0, 3)
    else: rect(c, 2, 1, 4, 3, 3)
    return c
def oilspot(w, h):                                       # a flat dark slick
    c = canvas(w, h); hh = max(2, h // 2)
    ell(c, w / 2.0, h - hh / 2.0, w / 2.0, hh / 2.0, 1)
    ell(c, w * 0.38, h - hh * 0.65, max(1, w * 0.12), max(0.6, hh * 0.15), 3)
    return c
def shell(w, h):                                         # a red shell with a shine
    c = canvas(w, h)
    ell(c, w / 2.0, h / 2.0, w / 2.0, h / 2.0, 1)
    ell(c, w / 2.0, h / 2.0, max(1, w / 2.0 - 1), max(1, h / 2.0 - 1), 2)
    ell(c, w * 0.38, h * 0.35, max(0.7, w * 0.14), max(0.7, h * 0.14), 3)
    return c
for nm, fn, ws in (('box', itembox, ((6, 6), (12, 12), (20, 16))), ('oil', oilspot, ((8, 4), (14, 7), (22, 10))), ('sh', shell, ((5, 5), (9, 9), (14, 14)))):
    U[nm + '0'] = add(sized(fn, 8, 16, *ws[0]))
    U[nm + '1'] = add(sized(fn, 16, 16, *ws[1]))
    U[nm + '2'] = add(sized(fn, 24, 16, *ws[2]))

spal = [0, c15(110, 70, 35), c15(35, 130, 50), c15(80, 190, 80),                      # 0 trees
        0, c15(70, 70, 80), c15(130, 130, 140), c15(190, 190, 200),                  # 1 rocks
        0, c15(30, 30, 40), c15(220, 50, 50), c15(255, 235, 140),                    # 2 red kart
        0, c15(30, 30, 40), c15(60, 100, 230), c15(255, 235, 140),                   # 3 blue kart
        0, c15(30, 30, 40), c15(240, 200, 40), c15(255, 255, 220),                   # 4 yellow kart
        0, c15(30, 30, 40), c15(60, 200, 90), c15(255, 255, 220),                    # 5 you (the body colour is replaced by your bug's shell colour)
        0, c15(60, 40, 10), c15(255, 200, 40), c15(255, 255, 255),                   # 6 item boxes
        0, c15(25, 25, 40), c15(230, 40, 40), c15(255, 255, 255)]                    # 7 shells and oil slicks

def arr(name, vals, typ='UINT8', per=16):
    s = 'static const %s %s[%d] = {\n' % (typ, name, len(vals))
    for i in range(0, len(vals), per): s += '    ' + ','.join(str(v) for v in vals[i:i + per]) + ',\n'
    return s + '};\n'

out = '/* generated by tools/make_kart.py - do not edit. Included by src/kart.c only (so it lives in the same bank). */\n'
out += '#define KT_N %d\n' % len(tiles)
out += arr('kt_bg', [b for t in tiles for b in enc(t)])
out += arr('kt_map', [v for r in mp for v in r], per=32)
out += arr('kt_attr', [v for r in at for v in r], per=32)
out += arr('kt_pal', pal, 'UINT16', 12)
out += arr('kt_hw', hwt, per=20) + arr('kt_qa', qa, per=20) + arr('kt_y', ytab, per=24)
out += arr('kt_bv', [(c * qa[2 * k]) >> 6 for c in range(-30, 30) for k in range(NR // 2)], 'INT8', 20)       # the bend of every road line pair for every curve (-30..29): no multiply at run time
sb = []
for u in units:
    for half in (0, 1):
        for yy in range(8):
            row = u[half * 8 + yy]; lo = hi = 0
            for x, v in enumerate(row):
                lo |= (v & 1) << (7 - x); hi |= ((v >> 1) & 1) << (7 - x)
            sb += [lo, hi]
out += '#define KU_N %d\n' % len(units)
out += arr('kt_spr', sb)
out += arr('kt_spal', spal, 'UINT16', 12)
for k, v in U.items(): out += '#define KU_%s %d\n' % (k.upper(), v)
open('src/kart_art.h', 'w').write(out)
print('tiles', len(tiles), 'units', len(units), 'bytes', len(tiles) * 16 + len(sb) + 16 * 32 * 2)
