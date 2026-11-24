#!/usr/bin/env python3
"""Exercise the actual GBC dungeon generator and labyrinth maker on a host."""
from pathlib import Path
import re
import subprocess
import tempfile
R=Path(__file__).resolve().parents[1]
src=(R/"src/dungeon_gen.c").read_text()
lab=(R/"src/lab.c").read_text()
assert "#define NMON      10" in (R/"src/dungeon_int.h").read_text()
def plain(s):
    return re.sub(r"(?m)^#(?:include|pragma)[^\n]*\n","",s)
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef int8_t INT8;
#define BANKED
#define DW 32
#define NROOM 8
#define NMON 10
enum { D_FLOOR,D_FLOOR2,D_WALL,D_FACE,D_DOWN,D_UP,D_CHEST,D_OPEN,D_CAGE,D_FREED,D_POTION,DG_NMT };
enum { K_SPIDER,K_BEETLE,K_WASP,K_SLIME,K_CRAWLER,K_GOLEM,K_MOTH,K_SPORE,K_SHADE,K_STINGER,K_CRAB,K_REAPER,K_QUEEN };
typedef struct { UINT16 x,y; UINT8 kind,hp,cool,flash,frame,alive; INT8 kx,ky; UINT8 st,t; INT8 lx,ly; } Mon;
typedef struct { UINT8 x,y,w,h; } Room;
static Room rooms[NROOM];
static Mon mon[NMON];
static UINT8 nrooms,depth,floors,kind,hell,pit,evt,lab,dn_x,dn_y;
static UINT16 bx,by,rng,seed;
static UINT8 dmap[DW*DW],scratch[1024];
static struct {UINT8 star,zone;} sv;
#define DM(x,y) dmap[((y)&31)*DW+((x)&31)]
#define WRAM_AT(base,type) ((type *)scratch)
static UINT16 world_seed(UINT8 star){ return seed ^ ((UINT16)star*257u); }
static UINT8 ev_roll(UINT8 dep){return 1+(dep+seed)%6;}
static UINT8 world_freed(UINT8 star){return star&1;}
static UINT8 hell_hp(UINT8 hp){return hp;}
static UINT8 bst_hp(UINT8 hp,UINT8 k,UINT8 world){(void)k;(void)world;return hp;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"seed %u floor %u: line %d %s\n",seed,depth,__LINE__,#x);return 1;}}while(0)
"""
tests=r"""
static int floor_tile(int x,int y){return DM(x,y)==D_FLOOR || DM(x,y)==D_FLOOR2;}
int main(void){
    unsigned count=0,maze=0,queens=0;
    for(unsigned a=0;a<8192;a++){
        seed=(UINT16)(a*7919u);sv.star=(UINT8)a;sv.zone=(UINT8)(a%12);
        floors=3+(a%4);pit=0;hell=(a%7)==0;kind=a%12;
        for(depth=1;depth<=floors;depth++){
            dg_make_floor();count++;maze+=lab!=0;
            CHECK(nrooms>=2 && nrooms<=NROOM);
            int upx=rooms[0].x+rooms[0].w/2,upy=rooms[0].y+1;
            CHECK(DM(upx,upy)==D_UP);
            CHECK(bx<512 && by<512 && floor_tile(bx>>4,by>>4));
            int live=0;
            for(int i=0;i<NMON;i++){
                if(!mon[i].alive)continue;
                int x=mon[i].x>>4,y=mon[i].y>>4;
                CHECK(x>0 && x<31 && y>0 && y<31 && floor_tile(x,y));
                CHECK(mon[i].hp>0);
                for(int j=0;j<i;j++)if(mon[j].alive)
                    CHECK(x!=(mon[j].x>>4) || y!=(mon[j].y>>4));
                if(mon[i].kind==K_QUEEN){CHECK(depth==floors && i==NMON-1);queens++;}
                live++;
            }
            CHECK(live<=NMON);
            if(depth<floors){CHECK(dn_x<32 && dn_y<32 && DM(dn_x,dn_y)==D_DOWN);}
            else{
                Room *r=&rooms[nrooms-1];CHECK(DM(r->x+r->w/2,r->y+1)==D_CAGE);
                CHECK(mon[NMON-1].alive && mon[NMON-1].kind==K_QUEEN && dn_x==255);
            }
            if((a&255)==0){
                UINT8 map_copy[sizeof dmap];Mon mon_copy[NMON];
                memcpy(map_copy,dmap,sizeof dmap);memcpy(mon_copy,mon,sizeof mon);
                dg_make_floor();CHECK(!memcmp(map_copy,dmap,sizeof dmap)&&!memcmp(mon_copy,mon,sizeof mon));
            }
        }
    }
    CHECK(queens==8192 && maze>1000);
    printf("GBC dungeon runtime: %u generated floors, %u labyrinths, %u queens, distinct spawn cells PASS\n",count,maze,queens);
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/"gen.c";exe=Path(tmp)/"gen"
    c.write_text(prefix+"\n"+plain(lab)+"\n"+plain(src)+"\n"+tests)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=address,undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
