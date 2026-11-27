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
typedef uint32_t UINT32;
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
    "static UINT8 bj_ok(UINT8 t, UINT8 tier, const XA *x)",
    "static UINT8 bj_offer_type(UINT16 r, UINT8 offer, UINT8 tier, const XA *x)",
    "static UINT8 gossip_pick(UINT32 *heard, UINT8 start)",
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
    unsigned checks=0;
    for(unsigned tier=1;tier<=3;tier++)for(unsigned mask=0;mask<32;mask++){
        fish_count=(mask&1)?240:1;x.petn=(mask&2)?240:1;
        sv.friends=(mask&4)?245:1;sv.deepest=(mask&8)?88:1;
        x.race=(mask&16)?250:1;
        for(unsigned seed=0;seed<256;seed++){
            UINT8 a=bj_offer_type(seed,0,tier,&x),b=bj_offer_type(seed,1,tier,&x),c=bj_offer_type(seed,2,tier,&x);
            CHECK(a!=b&&a!=c&&b!=c);
            CHECK(a>=1&&a<=8&&b>=1&&b<=8&&c>=1&&c<=8);
            CHECK(bj_ok(a,tier,&x)&&bj_ok(b,tier,&x)&&bj_ok(c,tier,&x));
            checks++;
        }
    }
    /* Old saves used 24 bits: new rumors must all be discoverable. */
    UINT32 heard=0xffffffu;
    for(unsigned i=0;i<8;i++){
        UINT8 k=gossip_pick(&heard,(UINT8)(i*9));
        CHECK(k>=24&&k<32);
    }
    CHECK(heard==UINT32_MAX);
    CHECK(gossip_pick(&heard,31)==31 && heard==0x80000000u);
    for(unsigned seed=0;seed<256;seed++){
        heard=0;
        for(unsigned i=0;i<32;i++){
            UINT32 before=heard;
            UINT8 k=gossip_pick(&heard,(UINT8)(seed+i*17));
            CHECK(k<32&&!(before&(UINT32_C(1)<<k)));
            CHECK(heard==(before|(UINT32_C(1)<<k)));
        }
        CHECK(heard==UINT32_MAX);
    }
    puts("GBC rumors: 8192 unique discoveries and legacy-save continuation PASS");
    printf("GBC bounty progress and %u three-contract eligibility cases PASS\\n",checks);
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
