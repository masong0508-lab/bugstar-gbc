#!/usr/bin/env python3
"""Exercise GBC Bolt dialogue against the current world and recovered memories.

The GBC world map is procedural: story progress (sv.zone) is not the player's
current biome. Test the authentic world_freed and bolt_talk functions under
ASan/UBSan, including high star IDs, eight memories, and true-endgame advice.
No generated dialogue assets or save formats are changed.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
read=lambda x:(ROOT/"src"/x).read_text()
bolt=read("bolt.c")
save=read("save.c")
util=read("util.c")
mem=read("bolt_data.h")

def function(source, marker):
    match=re.search(re.escape(marker)+r"[^;{}]*\{",source)
    assert match, marker
    i=match.start(); start=source.index("{",i); depth,end=1,start+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[i:end]

talk=function(bolt,"void bolt_talk(")
say=function(bolt,"static void say(")
freed=function(save,"UINT8 world_freed(")
scat=function(util,"char *scat(")
u16s=function(util,"char *u16s(")
table=bolt[bolt.index("static const UINT8 wk_cls[12]"):bolt.index("/* Bolt's caller")]
assert "UINT8 freed = world_freed(sv.star)" in talk
assert "if(freed)" in talk and "SELECT A NEW STAR" in talk
assert "FOLLOW THE ARROW TO THE DOOR" in talk
assert "SELECT THE GALAXY CHART AND PICK THE BEAST" in talk
assert "WE SAVED A SHIP, NOT EVERY WORLD." in talk
assert "bolt_cls[wk_cls[bolt_wk % 12]" in talk
assert "bolt_zone[(sv.zone % 6)" not in talk, "GBA's zone story is not a GBC location"
assert "static const char *const bolt_mem[32]" in mem

stub=r"""
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
#define BANKED
#define ST_DONE 4
typedef struct { UINT8 fuel; } Elite;
typedef struct {
    UINT8 star,zone,cores,story,potions,freed[8],up[16];
    UINT16 hp,level;
    Elite el;
} Save;
static Save sv;
static UINT8 bolt_wk,bolt_chat;
static int spoken,overflow;
static char said[8][110];
static UINT8 weird_of(UINT8 star){(void)star;return 0;}
static UINT16 hud_hpmax(UINT16 level){(void)level;return 100;}
static UINT8 tank_max=14;
#define TANK_CAP() tank_max
static void dlg_say(const char *who,const char *s){
    if(strcmp(who,"BOLT") || strlen(s)>=110) overflow++;
    if(spoken<8){strncpy(said[spoken],s,sizeof said[0]-1);said[spoken][109]=0;}
    spoken++;
}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"GBC Bolt narrative line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
tests=r"""
static void reset(void){
    memset(&sv,0,sizeof sv); memset(said,0,sizeof said);
    spoken=overflow=0;bolt_wk=0;bolt_chat=0;
    sv.star=77;sv.zone=3;sv.hp=100;sv.potions=3;sv.el.fuel=14;
    for(int i=0;i<8;i++)sv.freed[i]=(UINT8)(i+180);
}
static int has(const char *s){for(int i=0;i<spoken&&i<8;i++)if(strstr(said[i],s))return 1;return 0;}
int main(void){
    reset();
    sv.cores=0;CHECK(world_freed(77)==0);bolt_talk();
    CHECK(spoken==3 && !overflow);
    CHECK(has("THIS WORLD'S QUEEN STILL HAS A CORE."));
    CHECK(has("FOLLOW THE ARROW TO THE DOOR."));
    CHECK(!has("THE ANTHILL."));
    reset();sv.cores=3;sv.freed[0]=77;
    CHECK(world_freed(77)==1);bolt_talk();
    CHECK(spoken==4 && !overflow);
    CHECK(has("THIS WORLD IS FREE."));
    CHECK(has("SELECT A NEW STAR."));
    CHECK(has("I REMEMBER:"));
    CHECK(!has("FOLLOW THE ARROW TO THE DOOR."));
    // Don't falsely consider entries after sv.cores freed.
    reset();sv.cores=1;sv.freed[0]=23;sv.freed[1]=77;
    CHECK(!world_freed(77));
    // Even the end of the 256-world grid must retain its correct identity.
    reset();sv.star=255;sv.cores=7;sv.freed[6]=255;bolt_talk();
    CHECK(has("SELECT A NEW STAR.") && !overflow);
    reset();sv.cores=8;sv.story=0;bolt_talk();
    CHECK(spoken==4 && !overflow && has("MY NAME IS DAWN."));
    CHECK(has("SELECT THE GALAXY CHART AND PICK THE BEAST."));
    CHECK(has("CORE GLOWS"));
    reset();sv.cores=8;sv.story=ST_DONE;bolt_talk();
    CHECK(spoken==3 && has("YOU CAN STILL CALL ME BOLT."));
    CHECK(has("NOT EVERY WORLD"));
    CHECK(!has("SELECT THE GALAXY CHART"));
    // Health/fuel/potion worries still lead with at most two warnings,
    // then the main quest hint, not the other way around.
    reset();sv.cores=4;sv.hp=10;sv.potions=0;sv.el.fuel=0;bolt_talk();
    CHECK(spoken==6 && has("THIS WORLD'S QUEEN STILL HAS A CORE."));
    CHECK(strstr(said[0],"HURT") && strstr(said[1],"FUEL"));
    // Sweep all star IDs, world types, core counts and ending states.
    int visits=0;
    for(int star=0;star<256;star++)
    for(int cls=0;cls<12;cls++)
    for(int cores=0;cores<=8;cores++)
    for(int ending=0;ending<=1;ending++){
        reset();sv.star=(UINT8)star;bolt_wk=(UINT8)cls;
        sv.zone=(UINT8)(star%6);sv.cores=(UINT8)cores;
        sv.story=ending?ST_DONE:0;
        for(int i=0;i<cores;i++)sv.freed[i]=(UINT8)((star+i+1)&255);
        if(cores&&((star+cls)&1))sv.freed[cores-1]=(UINT8)star;
        bolt_talk();
        CHECK(!overflow && spoken>=3 && spoken<=4);
        if(cores<8){
            CHECK(has(world_freed((UINT8)star)?"THIS WORLD IS FREE.":"THIS WORLD'S QUEEN STILL HAS A CORE."));
        } else if(ending) CHECK(has("YOU CAN STILL CALL ME BOLT."));
        else CHECK(has("MY NAME IS DAWN."));
        visits++;
    }
    printf("GBC Bolt: %d world/type/core/ending scenarios and reachable story directions PASS\n",visits);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="gbc-bolt-story-") as tmp:
    path=Path(tmp)/"bolt.c";exe=Path(tmp)/"bolt"
    path.write_text(stub+"\n"+table+"\n#include \"bolt_data.h\"\n"+
                    scat+"\n"+u16s+"\n"+freed+"\n"+say+"\n"+talk+"\n"+tests)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                    "-Wno-unused-variable","-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all","-I",str(ROOT/"src"),
                    str(path),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
