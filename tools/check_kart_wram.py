#!/usr/bin/env python3
"""Ensure Kart World WRAM aliases remain safe for the GBC stack budget.

Only the real ROM link map can measure fixed stack space: check_banks.py
enforces >=1024 bytes during each ROM build. This source audit keeps the
race-only scratch and the HBlank interrupt-visible road buffer separate.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
hdr = (root / "src/wram.h").read_text()
kart = (root / "src/kart.c").read_text()
kart2 = (root / "src/kart2.c").read_text()
fx = (root / "src/fx.c").read_text()
raster = (root / "src/raster.c").read_text()
gate = (root / "tools/check_banks.py").read_text()

def address(symbol):
    m = re.search(r"^#define " + symbol + r"\s+(.+)$", hdr, re.M)
    assert m, f"Missing WRAM symbol: {symbol}"
    v = m.group(1).split("/*")[0].strip()
    if v.startswith("0x"):
        return int(v, 16)
    if v.startswith("(") and v.endswith(")"):
        base, number = re.fullmatch(r"\((WRAM_[A-Z_]+)\s*\+\s*(\d+)\)", v).groups()
        return address(base) + int(number)
    return address(v)

fade = address("WRAM_FADE")
tab = address("WRAM_KART_TAB")
rev = address("WRAM_KART_REV")
pal = address("WRAM_KART_PAL")
end = address("WRAM_KART_END")
dungeon = address("WRAM_DMAP")
assert (fade, tab, rev, pal, end, dungeon) == (
    0xD720, 0xD720, 0xD7B0, 0xD7C0, 0xD7D8, 0xD820
), "Kart staging no longer fits inside 256-byte fade scratch"
assert end <= dungeon
assert "#define tab WRAM_AT(WRAM_KART_TAB, UINT8)" in kart
assert "#define rsg WRAM_AT(WRAM_KART_REV, INT8)" in kart
assert "#define kp  WRAM_AT(WRAM_KART_PAL, UINT16)" in kart
assert "static UINT8  tab[144]" not in kart
assert "static INT8 rsg[16]" not in kart
assert "static UINT16 kp[12]" not in kart
assert kart.count("memset(tab, 0, 144)") == 2, "A pointer sizeof would clear only two bytes on GBC"
assert "sizeof tab" not in re.sub(r"/\*.*?\*/", "", kart, flags=re.S)
assert not re.search(r"\bfade_(in|out)\s*\(", kart + kart2), "A race cannot fade over its scratch"
assert "#define orig WRAM_AT(WRAM_FADE, UINT8)" in fx
assert "#define work WRAM_AT(WRAM_FADE + 128, UINT8)" in fx
assert "UINT8 road_scx[144];" in raster, "HBlank ISR must read fixed RAM, not banked WRAM"
assert "SCX_REG = road_scx" in raster
assert "room < 0x400" in gate, "ROM memory gate needs >=1024-byte stack"
assert "char b[110];" not in kart  # no extra persistent scratch allocation

bolt = (root / "src/bolt.c").read_text()
assert "static void say(const char *t, char *b)" in bolt
assert "char b[110], *d;" in bolt
assert "static void say(const char *t){ char b[110]" not in bolt
print("GBC stack WRAM audit: 184 bytes of kart staging in race-only fade scratch;")
print("fixed-RAM road ISR untouched; Bolt's nested 110-byte copy removed; PASS")
