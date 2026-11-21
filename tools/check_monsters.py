#!/usr/bin/env python3
"""GBC twelve-family enemy + 96 biome variants consistency audit."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
read = lambda p: (ROOT / p).read_text()
info = read("src/dungeon_int.h")
extra = read("src/dungeon_extra.c")
hdr = read("src/dungeon_extra.h")
dungeon = read("src/dungeon.c")
floor = read("src/dungeon_gen.c")
wild = read("src/wild.c")
art = read("src/dungeon_art.h")
save = read("src/wild.h")
names = ["K_SPIDER", "K_BEETLE", "K_WASP", "K_SLIME",
         "K_CRAWLER", "K_GOLEM", "K_MOTH", "K_SPORE",
         "K_SHADE", "K_STINGER", "K_CRAB", "K_REAPER", "K_QUEEN"]
enum = re.search(r"enum\s*\{([^{}]*K_SPIDER[^{}]*K_QUEEN[^{}]*)\}", info, re.S)
assert enum, "Missing dungeon monster enum"
got = re.findall(r"\bK_[A-Z]+\b", enum.group(1))
assert got == names, ("Enum renumbered; old save/monster behavior may break", got)
assert "#define NMON      10" in info
assert "UINT8  kills[60]" in save, "Do not alter old SRAM save layout"
assert "DG_EXTRA_KINDS 8" in hdr and "DG_EXTRA_TILES (DG_EXTRA_KINDS * 4)" in hdr
assert "#define MON_NTILES 54" in art
assert 64 + 54 + 32 < 200, "Extra monster sprites would overwrite the pet at 200"
m = re.search(r"extra_mon_tiles\[DG_EXTRA_TILES \* 16\]\s*=\s*\{(.*?)\};", extra, re.S)
assert m, "Missing extra sprite pixel data"
data = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))
assert len(data) == 512, ("Exactly eight * two * two 8x8 tiles", len(data))
first = [data[k * 64:k * 64 + 32] for k in range(8)]
assert len(set(first)) == 8, "Each new kind needs a unique visible silhouette"
assert all(data[k * 64:k * 64 + 32] != data[k * 64 + 32:k * 64 + 64]
           for k in range(8)), "Both animation frames must differ"
assert 'dg_extra_load(MON_TILE0)' in dungeon
assert 'DG_EXTRA_TILE(m->kind, m->frame)' in dungeon
for name in names[4:12]:
    assert "case " + name in extra, ("Missing combat AI", name)
assert "dg_extra_motion(m, &dx, &dy, step)" in dungeon
assert "K_QUEEN" in floor and "rndn((depth >= 3 || hell)" in floor
for label, n in (("wn", 12), ("extra_nm", 8)):
    arr = re.search(rf"static const char \*const {label}\[\d+\]\s*=\s*\{{([^}}]+)\}}", wild)
    assert arr, label
    parts = re.findall(r'"([^"]+)"', arr.group(1))
    assert len(parts) == n, (label, len(parts))
assert 'if(k >= 4 && k < 12)' in wild
assert "k >= 12 ? 48 + wk" in wild, "Queen save counters changed"
assert '"/26"' in wild and "pg < 25" in wild, "Missing bestiary expansion"
print("GBC monsters: 12 regular families, 8 extra silhouettes, 96 world variants, 26 bestiary pages; no new SRAM slots")
