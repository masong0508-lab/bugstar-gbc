#!/usr/bin/env python3
"""Compile GBC bounty-office counter logic and test 16-bit rollover.

No copy of the bounty implementation: the actual C functions are extracted
from src/town3.c. C types intentionally match the Game Boy's integer widths.
"""
from pathlib import Path
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/town3.c").read_text()


def extract(signature):
    match = re.search(r"\b" + re.escape(signature) + r"\s*\{", source)
    assert match, f"Missing bounty implementation: {signature}"
    depth = 0
    for idx in range(match.end() - 1, len(source)):
        if source[idx] == "{":
            depth += 1
        elif source[idx] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():idx + 1]
    raise AssertionError(f"Unterminated C function {signature}")


header = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef struct { UINT8 petn,race,bt,bn,bq[2]; } XA;
static struct { UINT8 friends,deepest; struct { UINT16 kills; } el; } sv;
static UINT16 monster_kills, mined;
static UINT8 fish_count;
#define EX_KILLS monster_kills
#define EX_FISHN fish_count
static UINT16 xtra_ore(void){ return mined; }
#define CHECK(v) do{ if(!(v)){fprintf(stderr,"GBC bounty C test line %d: %s\n",__LINE__,#v);return 1;} }while(0)
static void base(XA *x, UINT16 n){ x->bq[0]=(UINT8)n; x->bq[1]=(UINT8)(n>>8); }
"""
snippets = "\n\n".join(extract(s) for s in [
    "static UINT16 cnt(UINT8 t, const XA *x)",
    "static UINT16 w16(const UINT8 *p)",
    "static UINT8 bj_done(const XA *x)",
])
tests = r"""
int main(void){
    XA x; memset(&x,0,sizeof x);
    /* A new kill job accepted at 65533 used to pay immediately because
       base+5 wrapped to 2. It should pay only after five new kills. */
    x.bt=1; x.bn=5; base(&x,65533u); monster_kills=65533u;
    CHECK(!bj_done(&x)); monster_kills=65535u; CHECK(!bj_done(&x));
    monster_kills=1; CHECK(!bj_done(&x));
    monster_kills=2; CHECK(bj_done(&x));
    /* Pirates and ore mining also have 16-bit lifetime counters. */
    x.bt=6; x.bn=2; base(&x,65535u); sv.el.kills=0;
    CHECK(!bj_done(&x)); sv.el.kills=1; CHECK(bj_done(&x));
    x.bt=5; x.bn=3; base(&x,65534u); mined=0;
    CHECK(!bj_done(&x)); mined=1; CHECK(bj_done(&x));
    /* Nonwrapping counters must not pay on a corrupted backwards save. */
    x.bt=2; x.bn=4; base(&x,220); fish_count=10;
    CHECK(!bj_done(&x)); fish_count=223; CHECK(!bj_done(&x));
    fish_count=224; CHECK(bj_done(&x));
    x.bt=3; x.bn=2; base(&x,240); x.petn=1;
    CHECK(!bj_done(&x)); x.petn=242; CHECK(bj_done(&x));
    x.bt=4; x.bn=1; base(&x,240); sv.friends=240;
    CHECK(!bj_done(&x)); sv.friends=241; CHECK(bj_done(&x));
    x.bt=7; x.bn=2; base(&x,5); sv.deepest=5;
    CHECK(!bj_done(&x)); sv.deepest=7; CHECK(bj_done(&x));
    x.bt=8; x.bn=1; base(&x,7); x.race=7;
    CHECK(!bj_done(&x)); x.race=8; CHECK(bj_done(&x));
    x.bt=0; CHECK(!bj_done(&x));
    puts("GBC bounty progress: kill, pirate, ore rollover and all nonwrapping jobs PASS");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="bugstar-gbc-bounty-") as tmp:
    src = Path(tmp) / "bounty.c"
    exe = Path(tmp) / "bounty"
    src.write_text(header + "\n" + snippets + "\n" + tests)
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
