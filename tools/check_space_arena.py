#!/usr/bin/env python3
"""Exercise the production arena scheduler without a graphics dependency."""
from pathlib import Path
import tempfile, subprocess
source=(Path(__file__).resolve().parents[1]/"src/flight3d.c").read_text()
def extract(sig):
    a=source.index(sig); i=source.index("{",a)+1; depth=1
    while depth:
        depth+=(source[i]=="{")-(source[i]=="}");i+=1
    return source[a:i]
prefix=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
#define NP3 5
#define SFX_FANFARE 1
typedef struct {UINT8 st,kind,hp;} P3;
static P3 pir[NP3];
static struct {UINT16 level;} sv;
static UINT8 ar_wave,ar_left,ar_bombs,ar_gap,ar_pause,ar_boss;
static UINT32 ar_gold;
static int spawned,flagships,live;
static void say3(const char*s){assert(strlen(s)<=20);}
static void sfx(int v){(void)v;}
static P3 *pirate_spawn(UINT8 boss){
    for(int i=0;i<NP3;i++)if(!pir[i].st){
        pir[i]=(P3){1,boss,1};spawned++;flagships+=boss;live++;
        assert(live<=NP3);return &pir[i];
    }
    return 0;
}
"""
checks=r"""
int main(void){
    for(int wave=1;wave<=99;wave++){
        int n=ar_roster(wave);
        assert(n>=2&&n<=8);
        assert(ar_is_boss(wave)==(wave%4==0||wave==99));
        assert(ar_spacing(wave)>=30&&ar_spacing(wave)<=80);
    }
    assert(ar_roster(99)==8&&ar_roster(3)==3);
    ar_wave=1;ar_bombs=2;sv.level=20;wave_start();
    UINT32 expected=0;int total=0;
    for(int wave=1;wave<=99;wave++){
        assert(ar_wave==wave);
        int before=spawned,boss_before=flagships;
        int loops=0;
        while(ar_wave==wave){
            ar_spawn();
            if(live){memset(pir,0,sizeof pir);live=0;}
            assert(++loops<1500);
        }
        assert(spawned-before==ar_roster(wave));
        assert(flagships-boss_before==ar_is_boss(wave));
        expected+=20+8*wave;
        /* exercise production-sized kill rewards, including crossing 65535 */
        UINT32 reward=1000u*ar_roster(wave);
        ar_gold+=reward;expected+=reward;
        assert(ar_gold==expected);
        assert(ar_bombs<=3);
        total+=spawned-before;
    }
    assert(ar_wave==100&&flagships==25&&ar_gold>65535u);
    printf("Arena: all 99 waves, %d enemies, 25 flagships, 32-bit reward total PASS\n",total);
}
"""
assert "static UINT32 ar_gold;" in source
assert "if(++ar_wave==100)return" in source
assert "cleared = ar_wave - 1" in source
assert 'd = u32s(d, ar_gold)' in source
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"arena.c";exe=Path(td)/"arena"
    c.write_text(prefix+"\n".join(extract(s) for s in ("static UINT8 ar_is_boss(","static UINT8 ar_roster(","static UINT8 ar_spacing(","static void wave_start(","static void ar_spawn("))+checks)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
