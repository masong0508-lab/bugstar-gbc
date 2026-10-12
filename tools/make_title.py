#!/usr/bin/env python3
"""Renders the BUGSTAR title screen at native GBC size (160x144) -> tools/title.png.
Same planet lighting as the GBA gen.py, but drawn directly at 160x144 instead of
downscaling the 240x160 dithered picture. Then run: python3 tools/img2gbc.py"""
import numpy as np
from PIL import Image
W, H = 160, 144
B4 = np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]]) / 16.0
def c5(r,g,b): return (r & 0xF8, g & 0xF8, b & 0xF8)      # snap to GBC 5-bit steps

SP = [(6,6,16),(18,16,38),(40,34,70)]                   # space ramp (3 shades: palette budget)
STAR, STAR_DIM = (240,236,255), (120,110,160)
ATM = [(30,62,60),(60,110,90)]
G5 = [(10,22,18),(30,62,40),(68,118,62),(132,176,92),(214,232,160)]
def ramp(keys, n):
    out = []
    for k in range(n):
        t = k/(n-1)*(len(keys)-1); i = min(len(keys)-2, int(t)); f = t-i
        out.append(tuple(int(keys[i][c]*(1-f)+keys[i+1][c]*f) for c in range(3)))
    return out
GR = ramp(G5, 6)

yy, xx = np.mgrid[0:H, 0:W]
dith = B4[yy % 4, xx % 4]
def q(v, n):                                              # ordered-dither a 0..n-1 float to ints
    v = np.clip(v, 0, n-1); lo = np.floor(v); return np.clip(lo + ((v-lo) > dith), 0, n-1).astype(int)

cx, cy, R = 80, 212, 152
dx, dy = (xx-cx)/R, (yy-cy)/R; d2 = dx*dx+dy*dy; inside = d2 < 1
nz = np.sqrt(np.clip(1-d2, 0, 1)); L = np.array([-.5,-.65,.57]); L /= np.linalg.norm(L)
lam = np.clip(dx*L[0]+dy*L[1]+nz*L[2], 0, 1)
pat = (np.sin(dx*9+dy*3)+np.sin(dy*11-nz*5)+np.sin(nz*13+dx*4))/3
v = 0.35+3.4*lam+0.08*pat
v = np.where(d2 > .86, v-.9*(d2-.86)/.14, v)
gi = q(np.clip(v/4*5, 0, 5), 6)

rng = np.random.default_rng(11)
img = np.zeros((H, W, 3), np.uint8)
si = q(np.clip(0.3 + (yy/55.0)*1.9, 0, 2), 3)             # space: darkest at top, lighter at the horizon
for y in range(H):
    for x in range(W): img[y, x] = SP[si[y, x]]
# stars (only in space rows; each 1px)
for _ in range(32):
    x, y = int(rng.integers(0, W)), int(rng.integers(2, 100))
    if d2[y, x] < 1.25 or (36 <= x < 124 and 8 <= y < 56): continue     # keep clear of the planet and the text
    img[y, x] = STAR                                      # one star colour: the game twinkles it with its palette
# atmosphere ring
img[(d2 >= 1) & (d2 < 1.03)] = ATM[1]
g1 = (d2 >= 1.03) & (d2 < 1.075) & (((xx+yy) & 1) == 0); img[g1] = ATM[1]
g2 = (d2 >= 1.075) & (d2 < 1.12) & (((xx+yy) & 3) == 0); img[g2] = ATM[1]
for y in range(H):
    for x in range(W):
        if inside[y, x]: img[y, x] = GR[gi[y, x]]

# ---- tiny 5x7 font
F = {
'A':"01110 10001 10001 11111 10001 10001 10001",'B':"11110 10001 10001 11110 10001 10001 11110",
'C':"01110 10001 10000 10000 10000 10001 01110",'D':"11110 10001 10001 10001 10001 10001 11110",
'E':"11111 10000 10000 11110 10000 10000 11111",'F':"11111 10000 10000 11110 10000 10000 10000",
'G':"01110 10001 10000 10111 10001 10001 01111",'H':"10001 10001 10001 11111 10001 10001 10001",
'I':"01110 00100 00100 00100 00100 00100 01110",'J':"00111 00010 00010 00010 00010 10010 01100",
'K':"10001 10010 10100 11000 10100 10010 10001",'L':"10000 10000 10000 10000 10000 10000 11111",
'M':"10001 11011 10101 10101 10001 10001 10001",'N':"10001 11001 10101 10011 10001 10001 10001",
'O':"01110 10001 10001 10001 10001 10001 01110",'P':"11110 10001 10001 11110 10000 10000 10000",
'Q':"01110 10001 10001 10001 10101 10010 01101",'R':"11110 10001 10001 11110 10100 10010 10001",
'S':"01111 10000 10000 01110 00001 00001 11110",'T':"11111 00100 00100 00100 00100 00100 00100",
'U':"10001 10001 10001 10001 10001 10001 01110",'V':"10001 10001 10001 10001 10001 01010 00100",
'W':"10001 10001 10001 10101 10101 10101 01010",'X':"10001 10001 01010 00100 01010 10001 10001",
'Y':"10001 10001 01010 00100 00100 00100 00100",'Z':"11111 00001 00010 00100 01000 10000 11111",
'.':"00000 00000 00000 00000 00000 01100 01100",' ':"00000 00000 00000 00000 00000 00000 00000"}
def text(s, x, y, col, sc=1, shadow=None):
    for dxy in (([(sc,sc)] if shadow else []) + [(0,0)]):
        cc = shadow if dxy != (0,0) else col
        px = x
        for ch in s:
            for j, row in enumerate(F[ch].split()):
                for i, b in enumerate(row):
                    if b == '1':
                        img[y+dxy[1]+j*sc:y+dxy[1]+(j+1)*sc, px+dxy[0]+i*sc:px+dxy[0]+(i+1)*sc] = cc
            px += 6*sc
def tw(s, sc=1): return len(s)*6*sc-sc
def ctext(s, y, col, sc=1, shadow=None): text(s, (W-tw(s, sc))//2, y, col, sc, shadow)

GOLD, DG, WHITE, CYAN = (255,210,90), (60,40,10), (232,230,222), (140,220,255)
ctext("BUGSTAR", 12, GOLD, 2, DG)
ctext("A LITTLE BUG", 37, WHITE)
ctext("A BIG GALAXY", 47, WHITE)
# menu panel, aligned to the 8px tile grid: tiles x5..14, y11..15. Four lines, 9px apart (title.c MENU_Y0/MENU_DY)
PANEL, EDGE = (18,16,38), (255,210,90)
img[88:128, 40:120] = PANEL
img[88, 40:120] = EDGE; img[127, 40:120] = EDGE; img[88:128, 40] = EDGE; img[88:128, 119] = EDGE
for i, name in enumerate(("CONTINUE", "NEW GAME", "LOAD GAME", "OPTIONS")):
    ctext(name, 90 + i*9, WHITE)
img = (img & 0xF8).astype(np.uint8)
Image.fromarray(img).save("tools/title.png")
print("wrote tools/title.png")
