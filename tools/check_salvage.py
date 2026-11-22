#!/usr/bin/env python3
"""GBC space salvage regression: execute real shared pickup logic under ASan/UBSan.

Both flat and 3D flight retain two pod slots; supply types reuse the existing
8-bit goods field. Every power, fuel, gold and hold mutation is bounded and
neither view can mistake a supply type for an 0..7 commodity index.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
get=lambda p:(ROOT/"src"/p).read_text()
salv=get("salvage.h")
flat=get("space_foe.c")
flight=get("flight3d.c")
draw=get("space3d.c")
assert "#define NPOD 2" in flat and "#define NPOD3    2" in get("space3d_int.h")
assert "UINT8 g, life;" in flat and "UINT8 life, g;" in get("space3d_int.h")
for s in (flat,flight,draw):
    assert '#include "salvage.h"' in s
assert "r==0 ? SALV_FUEL : r==1 ? SALV_REPAIR" in flat
assert "r==0?SALV_FUEL:r==1?SALV_REPAIR" in flight
assert "salvage_take(p->g,amount,&got)" in flat
assert "salvage_take(q->g,n,&got)" in flight
assert "p->life=0; sfx(SFX_PICK);" in flat
assert "q->life=0; sfx(SFX_PICK);" in flight
assert "p->g==SALV_FUEL?5:p->g==SALV_REPAIR?6:2" in flat
assert "q->g==SALV_FUEL?PAL_MISC:q->g==SALV_REPAIR?PAL_ORB:PAL_LAS" in draw

def body(source, signature):
    i=source.index(signature);j=source.index("{",i);n=1;k=j+1
    while n:
        n+=(source[k]=="{")-(source[k]=="}")
        k+=1
    return source[i:k]

pick=body(salv,"static UINT8 salvage_take(")
decl=salv[salv.index("#define SALV_FUEL"):salv.index("/* Return the kind")]
harness=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef struct {
    struct { UINT8 cargo[8]; UINT8 fuel; } el;
    UINT16 level, hp, mp;
    UINT8 up[16];
    UINT32 gold;
} Save;
static Save sv;
static UINT8 hold_cap=10,tank_cap=14;
#define HOLD_CAP() hold_cap
#define TANK_CAP() tank_cap
static UINT16 hmax=220,mmax=140;
static UINT16 hud_hpmax(UINT16 level){(void)level;return hmax;}
static UINT16 hud_mpmax(UINT16 level){(void)level;return mmax;}
static UINT8 econ_load(void){UINT8 n=0;for(int i=0;i<8;i++)n+=sv.el.cargo[i];return n;}
#define CHECK(c) do {if(!(c)){fprintf(stderr,"GBC salvage line %d: %s\n",__LINE__,#c);return 1;}} while(0)
"""
cases=r"""
int main(void){
    UINT8 got=0,result=0;
    memset(&sv,0,sizeof sv);
    // Fuel on both the default and maximum upgraded tank.
    for(UINT8 cap=14;cap<=60;cap=(UINT8)(cap+23)){
        tank_cap=cap;sv.el.fuel=0;
        result=salvage_take(SALV_FUEL,1,&got);
        CHECK(result==SALV_GAS && got==6 && sv.el.fuel==6);
        sv.el.fuel=cap-2;result=salvage_take(SALV_FUEL,3,&got);
        CHECK(result==SALV_GAS && got==2 && sv.el.fuel==cap);
        result=salvage_take(SALV_FUEL,1,&got);
        CHECK(result==SALV_GOLD && got==15 && sv.el.fuel==cap);
    }
    sv.hp=200;sv.mp=20;hmax=220;mmax=140;
    result=salvage_take(SALV_REPAIR,1,&got);
    CHECK(result==SALV_HP && got==12 && sv.hp==212 && sv.mp==20);
    result=salvage_take(SALV_REPAIR,1,&got);
    CHECK(result==SALV_HP && got==8 && sv.hp==220);
    result=salvage_take(SALV_REPAIR,1,&got);
    CHECK(result==SALV_MP && got==10 && sv.mp==30);
    sv.mp=139;result=salvage_take(SALV_REPAIR,1,&got);
    CHECK(result==SALV_MP && got==1 && sv.mp==140);
    result=salvage_take(SALV_REPAIR,1,&got);
    CHECK(result==SALV_GOLD && got==15);
    // A trade good still works and respects the hold and full-hold fallback.
    memset(sv.el.cargo,0,sizeof sv.el.cargo);hold_cap=10;
    sv.el.cargo[0]=8; result=salvage_take(4,3,&got);
    CHECK(result==SALV_GOODS && got==2 && sv.el.cargo[4]==2);
    result=salvage_take(4,3,&got);
    CHECK(result==SALV_GOLD && got==10 && sv.el.cargo[4]==2);
    // All eight trade goods, including the highest index.
    memset(sv.el.cargo,0,sizeof sv.el.cargo);hold_cap=99;
    for(UINT8 i=0;i<8;i++){
        result=salvage_take(i,3,&got);
        CHECK(result==SALV_GOODS && got==3 && sv.el.cargo[i]==3);
    }
    // Unknown sentinel values must not index the cargo array.
    UINT32 previous=sv.gold;
    result=salvage_take(252,3,&got);
    CHECK(result==SALV_GOLD && got==10 && sv.gold==previous+10);
    result=salvage_take(255,2,&got);
    CHECK(result==SALV_GOLD && got==10 && sv.gold==previous+20);
    // Vary all tank/HP/MP/hold constraints with deterministic input.
    UINT32 z=0x9e3779b9U;
    for(int k=0;k<160000;k++){
        z=z*1664525u+1013904223u;
        tank_cap=(UINT8)(14+(z%47u));
        hold_cap=(UINT8)(10+((z>>8)%90u));
        sv.el.fuel=(UINT8)((z>>13)%100u);
        hmax=(UINT16)(100+(z&255u)); mmax=(UINT16)(50+((z>>9)&255u));
        sv.hp=(UINT16)((z>>7)%(hmax+1u));
        sv.mp=(UINT16)((z>>6)%(mmax+1u));
        for(int i=0;i<8;i++)sv.el.cargo[i]=0;
        UINT8 v=(UINT8)((z>>24)&15u);
        UINT8 typ=v==0?SALV_FUEL:v==1?SALV_REPAIR:(v&7u);
        UINT8 before=sv.el.fuel;
        result=salvage_take(typ,1+(z%3u),&got);
        CHECK(got>0);
        CHECK(sv.hp<=hmax && sv.mp<=mmax);
        CHECK(sv.el.fuel<=tank_cap || sv.el.fuel==before);
        CHECK(result<=SALV_GOLD);
        CHECK(econ_load()<=hold_cap);
    }
    puts("GBC salvage: 160000 constrained pickups, both views, fuel/hull/shield/hold bounds PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="gbc-salvage-") as temp:
    source=Path(temp)/"salvage.c"; exe=Path(temp)/"salvage"
    source.write_text(harness+"\n"+decl+"\n"+pick+"\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                    "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                    str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
