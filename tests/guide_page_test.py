#!/usr/bin/env python3
"""GBC readable HUD guide regression; no ROM font overflow or fake HUD counters."""
import ast
from pathlib import Path
import re

src=Path("tools/make_guide.py").read_text()
hdr=Path("src/guide_pages.h").read_text()
segment=src.split("PAGES = [",1)[1].split("\nbad = [",1)[0]
pages=ast.literal_eval("["+segment)
assert len(pages)==18, ("guide page count",len(pages))
assert '#define GUIDE_N 18' in hdr
assert len(re.findall(r'\("INSTRUMENTS","HEART STAR SHIP COIN"',src))==1
titles=[t for t,_,_ in pages]
assert titles.index("INSTRUMENTS")==titles.index("IN SPACE")+1
for title,icons,lines in pages:
    assert len(title)<=20, title
    assert len(icons.split())==4, title
    assert len(lines)==9,(title,len(lines))
    for line in lines: assert len(line)<=20,(title,line)
for label in ("HEART: YOUR HEALTH","IN SPACE, STAR BAR","MEANS SHIP SHIELDS.",
              "STAR: MAGIC POINTS","COIN: MONEY YOU OWN","LV: YOUR LEVEL."):
    assert label in hdr and label in src,label
# The guide dots use x=1..GUIDE_N. At 18, all dots remain in the
# 20-tile-wide (160px) GBC view, with no new sprite allocations.
assert len(pages)<20
print("GBC HUD help: 18 indexed pages, 20-column text, instrument meanings PASS")
