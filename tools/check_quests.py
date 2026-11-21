#!/usr/bin/env python3
"""Execute the *real* GBC city quest functions on a host compiler.

The 8-bit versus 16-bit mismatch matters on the Game Boy Color: a player
can visit all 256 stars, but one saved quest counter is eight bits. This
extracts C directly from src/town.c and uses narrow GBC-sized integer types
rather than reimplementing the quest logic in Python.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
town = (ROOT / "src/town.c").read_text()
data = (ROOT / "src/bolt_data.h").read_text()
generator = (ROOT / "tools/make_bolt.py").read_text()
assert "SELECT AGAIN TO DROP" in town, "town quest abandonment must require confirmation"
assert "if(HIT(J_SELECT) && have && !ok)" in town, "a completed job must not be accidentally dropped"


def function(signature):
    match = re.search(r"\b" + re.escape(signature) + r"\s*\{", town)
    assert match, f"missing game function: {signature}"
    start = match.start()
    depth = 0
    for idx in range(match.end() - 1, len(town)):
        if town[idx] == "{":
            depth += 1
        elif town[idx] == "}":
            depth -= 1
            if depth == 0:
                return town[start:idx + 1]
    raise AssertionError(f"unterminated function {signature}")


def quest_c_source():
    head = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef struct { UINT8 rsv[8], cargo[8]; } Elite;
static struct { UINT8 friends, deepest, zone, star, rsv[4]; Elite el; } sv;
static struct { UINT8 seen[32], pit; } ex;
static UINT16 th;
static UINT8 qt, qa, qb, qs;
static UINT16 qr;
static UINT8 fish_count;
static UINT16 kills_count;
#define EX_FISHN fish_count
#define EX_KILLS kills_count
#define Q_T sv.el.rsv[2]
#define Q_A sv.el.rsv[3]
#define Q_B sv.el.rsv[4]
#define Q_S sv.el.rsv[5]
#define Q_N sv.el.rsv[6]
#define Q_R (sv.rsv[0] | ((UINT16)sv.rsv[1] << 8))
static UINT16 econ_base(UINT8 t){ (void)t; return 25; }
#define ASSERT(cond) do { if(!(cond)){ \
    fprintf(stderr,"GBC quest regression line %d: %s\n",__LINE__,#cond); return 1; \
} } while(0)
static void visited(unsigned n) {
    memset(ex.seen,0,sizeof ex.seen);
    for(unsigned j=0;j<n;j++) ex.seen[j>>3] |= (UINT8)(1u<<(j&7));
}
"""
    snippets = [function(s) for s in [
        "static UINT16 seen_n(void)",
        "static void q_offer(UINT8 star, UINT8 idx, UINT8 tier)",
        "static void q_load(void)",
        "static UINT8 q_done(UINT8 idx)",
    ]]
    test = r"""
int main(void){
    unsigned seen, i, inspected=0, land_jobs=0, converted=0;
    for(seen=0; seen<=256; seen++){
        visited(seen);
        ASSERT(seen_n()==seen);
        ex.pit=9; sv.deepest=4; sv.friends=3;
        for(i=0;i<8192;i+=19){
            th=(UINT16)i; Q_N=3; Q_T=0;
            q_offer(1,0,4);
            ASSERT(qt!=8);  /* no unplayable pit-wave-11 contract */
            if(qt==7){
                land_jobs++;
                ASSERT(seen<256);
                ASSERT(qa>=1 && qa<=3);
                ASSERT(qb==seen);
                ASSERT((UINT16)qb+qa<=256);
                ASSERT(!q_done(0));
            } else if(seen==256 && qt==5) converted++;
            inspected++;
        }
    }
    ASSERT(land_jobs>100 && converted>0);
    /* 256th world must count even though the saved base is one byte. */
    visited(255); Q_T=7; Q_A=1; Q_B=255; Q_S=1; qt=7; qa=1; qb=255;
    ASSERT(!q_done(0));
    visited(256);
    ASSERT(q_done(0));
    /* An older save could ask for the 257th/258th world. Repair its
       requirement without resetting its already collected progress or gold. */
    visited(254); Q_T=7; Q_A=4; Q_B=254;
    sv.rsv[0]=233; sv.rsv[1]=1; q_load();
    ASSERT(qa==2 && Q_A==2 && qb==254 && qr==489);
    ASSERT(!q_done(0));
    visited(255); ASSERT(!q_done(0));
    visited(256); ASSERT(q_done(0));
    /* Once all worlds are visited the board must offer a meaningful job. */
    visited(256);
    for(i=0;i<65536;i+=37){ th=(UINT16)i; Q_N=0; q_offer(0,0,4); ASSERT(qt!=7); }
    /* Normal campaign dungeon depth is 3..6. Town boards must not
       require endless-mode floors just because a player cleared B6. */
    visited(10); sv.zone=0; sv.deepest=3;
    for(i=0;i<8192;i+=17){
        th=(UINT16)i; Q_N=1; q_offer(1,0,2); ASSERT(qt!=2);
    }
    sv.zone=0; sv.deepest=2;
    for(i=0;i<8192;i+=17){
        th=(UINT16)i; Q_N=1; q_offer(1,0,2);
        if(qt==2) ASSERT(qa==3);
    }
    sv.zone=3; sv.deepest=5;
    for(i=0;i<8192;i+=17){
        th=(UINT16)i; Q_N=1; q_offer(1,0,2);
        if(qt==2) ASSERT(qa==6);
    }
    sv.zone=3; sv.deepest=6;
    for(i=0;i<8192;i+=17){
        th=(UINT16)i; Q_N=1; q_offer(1,0,2); ASSERT(qt!=2);
    }
    /* A previous build may have left an accepted impossible floor-B92
       contract on the cartridge. Repair it without losing the reward. */
    sv.zone=0; sv.deepest=2; Q_T=2; Q_A=92;
    sv.rsv[0]=77; sv.rsv[1]=2; q_load();
    ASSERT(qa==3 && Q_A==3 && qr==589 && !q_done(0));
    sv.deepest=3; ASSERT(q_done(0));
    printf("GBC quest runtime: %u combinations, %u exploration jobs, impossible offers 0; PASS\n",
           inspected,land_jobs);
    return 0;
}
"""
    return head + "\n\n".join(snippets) + "\n" + test


# Story progression is also gameplay: an early clue cannot give away the
# specific final word that the Technobeast encounter is built around.
memory = re.search(r"static const char \*const bolt_mem\[32\] = \{(.*?)\};", data, re.S)
assert memory, "missing Bolt's recovered memories"
lines = re.findall(r'"([^"]+)"', memory.group(1))
assert len(lines) == 32
assert lines[0] == "I WAS A SHIP."
assert lines[1] == "A NAME WAS PAINTED ON MY HULL.", "Bolt's first core spoils his real name"
assert "I WAS CALLED THE DAWN." not in lines[:28]
assert any("MY NAME" in line for line in lines[-4:]), "eighth core must resolve the hidden name"
assert "A NAME WAS PAINTED ON MY HULL." in generator, "regenerating data would undo the clue"

with tempfile.TemporaryDirectory(prefix="bugstar-gbc-quests-") as tmp:
    src = Path(tmp) / "quests.c"
    exe = Path(tmp) / "quests"
    src.write_text(quest_c_source())
    subprocess.run([
        "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=undefined", "-fno-sanitize-recover=all",
        str(src), "-o", str(exe),
    ], check=True)
    subprocess.run([str(exe)], check=True)
print("GBC story clue order: PASS")
