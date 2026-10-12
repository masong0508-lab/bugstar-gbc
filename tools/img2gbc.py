#!/usr/bin/env python3
"""img2gbc: 160x144 PNG -> GBC background data (GBDK-2020).
  python3 tools/img2gbc.py [in.png] [out_basename]      defaults: tools/title.png  src/title_art
Fits one 4-colour palette per 8x8 tile (max 8 palettes), dedupes tiles incl. h/v flips,
and writes <out>.c / <out>.h plus tools/<name>_check.png (what the GBC will actually show).
Tile data goes to VRAM bank 1 (bank 0 keeps the text font), so max 256 unique tiles."""
import sys, collections, numpy as np
from PIL import Image
src = sys.argv[1] if len(sys.argv) > 1 else "tools/title.png"
out = sys.argv[2] if len(sys.argv) > 2 else "src/title_art"
name = out.split("/")[-1]
im = np.array(Image.open(src).convert("RGB")).astype(int); H, W, _ = im.shape
assert W % 8 == 0 and H % 8 == 0
pix = ((im[..., 0] >> 3) | ((im[..., 1] >> 3) << 5) | ((im[..., 2] >> 3) << 10)).astype(int)
TW, TH = W // 8, H // 8
def rgb(p): return np.array([p & 31, (p >> 5) & 31, (p >> 10) & 31])
def dist(a, b): return int(((rgb(a) - rgb(b)) ** 2).sum())
def lum(p): r = rgb(p); return int(r[0] * 3 + r[1] * 6 + r[2])
freq = collections.Counter(pix.flatten().tolist())

def reduce(cols, k=4):
    """Pick k representative colours from a set (weighted by global use): furthest-first + 6 k-means passes."""
    cols = sorted(cols, key=lambda c: -freq[c])
    cen = [cols[0]]
    while len(cen) < min(k, len(cols)):
        cen.append(max(cols, key=lambda c: min(dist(c, d) for d in cen)))
    for _ in range(6):
        groups = {c: [] for c in cen}
        for c in cols: groups[min(cen, key=lambda d: dist(c, d))].append(c)
        new = []
        for c, g in groups.items():
            m = sum(rgb(x) * freq[x] for x in g) / sum(freq[x] for x in g)
            new.append(min(g, key=lambda x: float(((rgb(x) - m) ** 2).sum())))
        cen = new
    return set(cen)

tiles = {}; sets = {}
for ty in range(TH):
    for tx in range(TW):
        t = pix[ty*8:ty*8+8, tx*8:tx*8+8]; tiles[(tx, ty)] = t
        s = set(t.flatten().tolist()); sets[(tx, ty)] = reduce(s) if len(s) > 4 else s

pals = []
for s in sorted(sets.values(), key=lambda s: -len(s)):
    best, bc = None, 99
    for i, p in enumerate(pals):
        u = len(p | s)
        if u <= 4 and u - len(p) < bc: best, bc = i, u - len(p)
    if best is None: pals.append(set(s))
    else: pals[best] |= s
print('palettes before merge:',len(pals))
while len(pals) > 8:                      # too many: merge the closest pair and re-reduce
    i, j = min(((i, j) for i in range(len(pals)) for j in range(i+1, len(pals))),
               key=lambda ij: len(pals[ij[0]] | pals[ij[1]]) * 1000 + sum(
                   min(dist(a, b) for b in pals[ij[1]]) for a in pals[ij[0]]))
    u = pals[i] | pals[j]; pals[i] = reduce(u) if len(u) > 4 else u; del pals[j]
plist = []
for p in pals:
    c = sorted(p, key=lum)
    while len(c) < 4: c.insert(0, c[0])
    plist.append(c)

def fit(t, pl):
    d = np.array([[dist(int(v), c) for c in pl] for v in t.flatten()])
    return d.argmin(1).reshape(8, 8), int(d.min(1).sum())
assign = {}
for k, t in tiles.items():
    assign[k] = min(((i,) + fit(t, pl) for i, pl in enumerate(plist)), key=lambda r: r[2])

STAR = (240 >> 3) | ((236 >> 3) << 5) | ((255 >> 3) << 10)   # twinkling stars live in palettes whose colour 3 is this
def star_pals(): return [i for i, p in enumerate(plist) if p[3] == STAR]
for sp in list(star_pals()):                 # use spare palettes to split star tiles in two phases
    if len(plist) >= 8: break
    plist.append(list(plist[sp])); new = len(plist) - 1
    for k, (i, idx, e) in list(assign.items()):
        if i == sp and (k[0] + k[1]) & 1: assign[k] = (new, idx, e)

store, tdata, tmap, tattr, err = {}, [], [], [], 0
for ty in range(TH):
    for tx in range(TW):
        pi, idx, e = assign[(tx, ty)]; err += e
        found = None
        for fl, v in enumerate((idx, idx[:, ::-1], idx[::-1], idx[::-1, ::-1])):
            if v.tobytes() in store: found = (store[v.tobytes()], fl); break
        if not found:
            store[idx.tobytes()] = len(tdata); tdata.append(idx); found = (len(tdata) - 1, 0)
        tmap.append(found[0]); tattr.append(pi | 8 | ((found[1] & 1) << 5) | ((found[1] >> 1) << 6))
N = len(tdata)
print("palettes used: %d/8   unique tiles: %d/256 (of %d)   residual colour error: %d" % (len(plist), N, TW*TH, err))
if N > 256: sys.exit("too many unique tiles - simplify the art (fewer stars / less dither)")

chk = np.zeros((H, W, 3), np.uint8)
for ty in range(TH):
    for tx in range(TW):
        a = tattr[ty*TW+tx]; t = tdata[tmap[ty*TW+tx]]
        if a & 32: t = t[:, ::-1]
        if a & 64: t = t[::-1]
        pl = plist[a & 7]
        for y in range(8):
            for x in range(8): chk[ty*8+y, tx*8+x] = (rgb(pl[t[y, x]]) * 8).astype(np.uint8)
Image.fromarray(chk).save("tools/%s_check.png" % name)

def tile_bytes(t):
    b = []
    for y in range(8):
        lo = sum(((int(t[y, x]) & 1) << (7 - x)) for x in range(8)); hi = sum((((int(t[y, x]) >> 1) & 1) << (7 - x)) for x in range(8))
        b += [lo, hi]
    return b
def arr(ty, nm, vals, fmt, per):
    s = "const %s %s[%d] = {\n" % (ty, nm, len(vals))
    for i in range(0, len(vals), per): s += "    " + ",".join(fmt % v for v in vals[i:i+per]) + ",\n"
    return s + "};\n"
stars = star_pals()
cursor = ["11......", "1211....", "12221...", "122221..", "122221..", "12221...", "1211....", "11......"]
cur = []
for r in cursor:
    r = r.ljust(8, "."); v = [0 if c == "." else int(c) for c in r]
    cur += [sum((v[x] & 1) << (7 - x) for x in range(8)), sum(((v[x] >> 1) & 1) << (7 - x) for x in range(8))]
open(out + ".h", "w").write("""/* generated by tools/img2gbc.py from %s - do not edit */
#ifndef %s_DATA_H
#define %s_DATA_H
#include <gb/gb.h>
#include <gb/cgb.h>
#define TITLE_ART_W %d
#define TITLE_ART_H %d
#define TITLE_NTILES %d
#define TITLE_NSTAR %d
extern const UINT8 title_tiles[TITLE_NTILES * 16]; /* 2bpp, load into VRAM bank 1 */
extern const UINT8 title_map[TITLE_ART_W * TITLE_ART_H];    /* tile numbers (bank 0 map) */
extern const UINT8 title_attr[TITLE_ART_W * TITLE_ART_H];   /* palette | bank1 | flips (bank 1 map) */
extern const palette_color_t title_pal[8 * 4];
extern const UINT8 title_star_pal[%d];               /* palettes whose colour 3 twinkles */
extern const UINT8 title_cursor[16];                 /* 8x8 menu arrow, sprite palette: 0 clear,1 dark,2 gold,3 white */
#endif
""" % (src, name.upper(), name.upper(), TW, TH, N, len(stars), max(1, len(stars))))
pv = [c for p in plist for c in p] + [0] * (32 - 4 * len(plist))
open(out + ".c", "w").write('/* generated by tools/img2gbc.py from %s - do not edit */\n#include "%s.h"\n' % (src, name)
    + arr("UINT8", "title_tiles", [b for t in tdata for b in tile_bytes(t)], "0x%02X", 16)
    + arr("UINT8", "title_map", tmap, "%d", 20) + arr("UINT8", "title_attr", tattr, "0x%02X", 20)
    + arr("palette_color_t", "title_pal", pv, "0x%04X", 4)
    + arr("UINT8", "title_star_pal", stars or [0], "%d", 8) + arr("UINT8", "title_cursor", cur, "0x%02X", 8))
print("wrote %s.c / %s.h, tools/%s_check.png; star palettes: %s" % (out, out, name, stars))
