#!/usr/bin/env python3
"""Guard GBC's 20-column narrative, restored memories and Technobeast finale.

This is a source-level content/continuity test. Actual GBDK compilation and
PyBoy render coverage are separate checks in the cartridge workflow.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
ending = (root / "src/ending.c").read_text()
bolt = (root / "src/bolt.c").read_text()
mem = (root / "src/bolt_data.h").read_text()
planet = (root / "src/planet.c").read_text()
boss = (root / "src/space3d_boss.c").read_text()
feat = (root / "src/feat.c").read_text()
dungeon = (root / "src/dungeon.c").read_text()
dg_gen = (root / "src/dungeon_gen.c").read_text()
feat_h = (root / "src/feat.h").read_text()


def strings(section):
    return [s.replace(r'\"', '"') for s in re.findall(r'"((?:\\.|[^"\\])*)"', section)]


ending_page = ending.split("static const char *const page[3][9] = {", 1)[1].split(
    "static UINT8 slen(", 1
)[0]
pages = strings(ending_page)
assert len(pages) == 27, "the three nine-line ending panels changed unexpectedly"
assert all(len(s) <= 20 for s in pages), "ending text overflows 20-column GBC viewport"
assert "THE SHIP WAS DAWN." in pages, "true identity needs a precise payoff"
assert "THE KING TOOK THEM." in pages, "ending lost the antagonist's responsibility"
assert "TO WARM HIS NEST." in pages, (
    "GBC must establish the King's motive without excusing the kidnapping"
)
assert 'sv.friends ? "SOME FRIENDS MADE IT" : "FRIENDS STILL WAIT"' in ending, (
    "GBC epilogue must honor actual rescued-friend count"
)
assert all(len(line) <= 20 for line in ("SOME FRIENDS MADE IT", "FRIENDS STILL WAIT")), (
    "conditional post-finale lines must fit the 20-column viewport"
)
assert "if(spoken[0]) wait_a(" in ending, (
    "new conditional ending line must respect existing A/START pacing"
)
assert "YOU CARRIED BOLT" in pages, "ending has no player/companion connection"
assert any("YOU ALL HOME." in s for s in pages), "the ending must conclude Bolt's promise"
assert "i == 0 ? TXT_GOLD" in ending, "chapter intertitle contrast is missing"
assert "wait_a((p == 2 && i == 7)" in ending, (
    "final dialogue needs deliberate pacing without slowing ordinary gameplay"
)
assert "HIT(J_A) || HIT(J_START)" in ending, "ending cannot be skipped with either confirm"

m = re.search(r"static const char \*const bolt_mem\[32\] = \{(.*?)\};", mem, re.S)
assert m, "missing generated 8 x 4 memory table"
memory_lines = strings(m.group(1))
assert len(memory_lines) == 32
assert all(0 < len(s) <= 54 for s in memory_lines)
assert "A NAME WAS PAINTED ON MY HULL." in memory_lines[:4], (
    "memory one must establish the lost name without giving it away"
)
assert all("THE DAWN" not in s for s in memory_lines[:28]), (
    "GBC must not identify Bolt before the eighth core"
)
assert "THE KING CUT OPEN MY HEART." in memory_lines, (
    "GBC memory arcs must explain how the Technobeast was created"
)
assert "MY NAME HAS COME BACK AT LAST." in memory_lines[28:], (
    "GBC memory eight must pay off the mystery"
)
zone = re.search(r"static const char \*const bolt_zone\[12\] = \{(.*?)\};", mem, re.S)
assert zone, "GBC zone dialogue table is missing"
assert "THE DAWN" not in zone.group(1), (
    "GBC anthill dialogue must not reveal the eighth-core identity too early"
)


assert "if(cores < 8)" in bolt and "else if(!(sv.story & ST_DONE))" in bolt
assert 'say("I REMEMBER MY NAME. THE DAWN.' in bolt
assert "I AM DAWN. BUT YOU CAN STILL CALL ME BOLT." in bolt, (
    "post-finale companion must retain his name and personality"
)
assert "KEEP LOOKING FOR THE FRIENDS WE MISSED." in bolt, (
    "the GBC postgame must not claim every captive was rescued"
)
assert "WAIT FOR ITS CORE TO GLOW" in bolt, "companion no longer tells the player how to win"
assert "sv.cores < 8" in boss, "3D Technobeast must remain gated behind eight cores"

assert "void  boss_evidence(UINT8 core) BANKED;" in feat_h, (
    "the GBC clue reader must use the dedicated bank instead of filling bank 5"
)
clue_section = feat.split("void boss_evidence(UINT8 core) BANKED {", 1)[1].split(
    "void dungeon_won(UINT8 kind)", 1
)[0]
m = re.search(r"static const char \*const ledger\[8\] = \{(.*?)\};", clue_section, re.S)
assert m, "expected eight GBC Queen case files tied to recovered core count"
ledger = strings(m.group(1))
assert len(ledger) == 8
assert all(0 < len(s) < 110 for s in ledger), (
    "ledger dialogue must fit Bolt's existing 110-byte dialog buffer"
)
assert all("THE DAWN" not in s for s in ledger), (
    "the Queen records must not spoil the eighth memory's identity reveal"
)
assert "boss_evidence(sv.cores);" in dungeon, (
    "the queen's case file must play from a real gameplay chest pickup"
)
assert "boss_clue_seen = 1" in dungeon and "boss_clue_seen = 0" in dungeon, (
    "Queen-floor evidence must trigger once each visit, not per ordinary chest"
)
assert "depth == floors && !hell && !pit && !boss_clue_seen && !world_freed(sv.star)" in dungeon, (
    "evidence should belong to fresh story bosses, not endless-mode chests"
)
assert "depth == floors && !hell && nrooms > 1 && !world_freed(sv.star)" in dg_gen, (
    "the first Queen floor must physically contain a discoverable clue chest"
)
assert "DM(x, y) = D_CHEST; placed = 1;" in dg_gen, (
    "the case file must use existing solid chest tiles, not new image assets"
)


assert "I CAN FLY THIS SHIP, BUT I CAN'T REMEMBER MY LAST VOYAGE." in planet, (
    "GBC Bolt's opening lacks character motivation"
)
assert "LET'S FIND YOUR FRIENDS" in planet, "GBC opening has lost its rescue stakes"

print("GBC story: 20-column cinematic pacing, 8 memories, Dawn reveal, boss gate PASS")
