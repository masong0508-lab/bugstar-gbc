#!/usr/bin/env python3
"""Run Kart World's real item code with its production timer declarations.

The shield used to store 420 in a byte, reducing its protection to 164 ticks.
These checks exercise acquisition, every lifetime tick, collision costs,
expiry and rivals without emulating the unrelated LCD/road interrupt code.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/kart.c").read_text()


def function(signature):
    match = re.search(r"\b" + re.escape(signature) + r"\s*\{", source)
    assert match, f"missing real kart routine: {signature}"
    depth = 0
    for pos in range(match.end() - 1, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if not depth:
                return source[match.start():pos + 1]
    raise AssertionError(f"unterminated kart routine: {signature}")


state = source.split("/* items */", 1)[1].split("/* what is kept", 1)[0]
tables = source.split("static const UINT16 BXM", 1)[1].split("\n\n", 1)[0]
test = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;
typedef int8_t INT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
#define NRIV 3
#define LAPM 2048U
static UINT16 tm[1 + NRIV];
static INT16 vel[1 + NRIV], lx[1 + NRIV];
static UINT8 rivals_on, rev, last_sfx;
enum { SFX_BACK, SFX_HURT, SFX_LASER, SFX_LEVEL, SFX_SPARK,
       SFX_FIRE, SFX_SPLASH, SFX_PICK, SFX_BOOM };
static void sfx(UINT8 n){ last_sfx = n; }
static void rum_hum(UINT8 n){ (void)n; }
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"Kart item regression at %d: %s\n",__LINE__,#x); return 1; } }while(0)
"""
test += "\nstatic const UINT16 BXM" + tables + "\n" + state
for signature in (
    "static UINT8 krnd(void)",
    "static UINT8 place_now(void)",
    "static void hit_kart(UINT8 k, UINT8 big)",
    "static UINT8 drop_oil(UINT16 m, INT16 l)",
    "static void use_item(void)",
    "static void items_tick(UINT8 go)",
):
    test += "\n" + function(signature)

test += r"""
int main(void){
    unsigned n;
    (void)gh_on; (void)ghv;
    tm[0] = 50;  /* a clear stretch without an item box or opponent */
    hold = 8;
    use_item();
    CHECK(!hold && shield_t == 420 && last_sfx == SFX_SPARK);
    for(n = 1; n <= 420; n++){
        items_tick(1);
        CHECK(shield_t == 420 - n);
    }
    items_tick(1);
    CHECK(!shield_t);

    /* A shield preserves both speed and control when hit. */
    hold = 8; use_item(); vel[0] = 256;
    hit_kart(0, 1);
    CHECK(shield_t == 330 && !stun[0] && vel[0] == 256 && last_sfx == SFX_BACK);
    hit_kart(0, 0);
    CHECK(shield_t == 240 && !stun[0] && vel[0] == 256);
    hit_kart(0, 1); hit_kart(0, 1);
    CHECK(shield_t == 60);
    hit_kart(0, 0);
    CHECK(shield_t == 1 && !stun[0]);
    items_tick(1);
    hit_kart(0, 0);
    CHECK(!shield_t && stun[0] == 40 && vel[0] == 64 && last_sfx == SFX_HURT);

    /* A new pickup restores the full timer; rivals cannot borrow it. */
    stun[0] = 0; hold = 8; use_item();
    for(n = 0; n < 403; n++) items_tick(1);
    CHECK(shield_t == 17);
    hold = 8; use_item();
    CHECK(shield_t == 420);
    vel[1] = 240;
    hit_kart(1, 1);
    CHECK(stun[1] == 56 && !vel[1] && shield_t == 420);

    /* The real oil and shell collision paths both consume protection. */
    vel[0] = 256;
    drop_oil(tm[0], lx[0]);
    items_tick(1);
    CHECK(!oil[0].life && shield_t == 329 && !stun[0] && vel[0] == 256);
    hold = 8; use_item();
    shl.m = tm[0] - 7; shl.lane = lx[0]; shl.life = 3;
    shl.owner = 1; shl.tg = -1; shl.f = 0;
    items_tick(1);
    CHECK(!shl.life && shield_t == 329 && !stun[0] && vel[0] == 256);

    /* Busy projectile and trap pools preserve the held item and active
       objects. A player may retry when the original object expires. */
    memset(oil,0,sizeof oil); memset(&shl,0,sizeof shl);
    shl.life=30;shl.owner=1;shl.m=400;hold=2;
    use_item();CHECK(hold==2&&shl.life==30&&shl.m==400&&shl.owner==1);
    shl.life=0;use_item();CHECK(!hold&&shl.owner==0&&shl.life==240);
    for(n=0;n<3;n++)CHECK(drop_oil((UINT16)(400+n*40),0));
    hold=3;use_item();CHECK(hold==3&&oil[0].m==400&&oil[0].life==900);
    oil[1].life=0;use_item();CHECK(!hold&&oil[1].life==900&&oil[0].m==400);
    /* Expiry does not produce one last collision. */
    memset(oil,0,sizeof oil); memset(&shl,0,sizeof shl);
    shield_t=0;stun[0]=0;vel[0]=256;shl.life=1;shl.owner=1;shl.m=tm[0]-1;shl.tg=-1;shl.lane=lx[0];
    oil[0].life=1;oil[0].m=tm[0];oil[0].lane=lx[0];items_tick(1);
    CHECK(!stun[0]&&vel[0]==256&&!oil[0].life&&!shl.life);
    rivals_on=1;for(n=1;n<=NRIV;n++)tm[n]=tm[0];CHECK(place_now()==4);
    rivals_on=0;CHECK(place_now()==1);

    /* Timer storage changes must not alter either existing turbo item. */
    hold = 1; use_item(); CHECK(!hold && boost_t == 70);
    hold = 7; use_item(); CHECK(!hold && boost_t == 150);
    puts("GBC kart items: shield ticks, busy projectile/trap retention, expiry, ties, rivals and turbos PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-gbc-kart-items-") as tmp:
    cpath, executable = Path(tmp) / "test.c", Path(tmp) / "test"
    cpath.write_text(test)
    subprocess.run([
        "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=undefined", "-fno-sanitize-recover=all",
        str(cpath), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
