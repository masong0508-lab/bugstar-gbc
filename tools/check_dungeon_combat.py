#!/usr/bin/env python3
"""Exercise real GBC bite and split spawning with a host fixture."""
from pathlib import Path
import re
import subprocess
import tempfile
R=Path(__file__).resolve().parents[1]
s=(R/"src/dungeon.c").read_text()
extra=(R/"src/dungeon_extra.c").read_text()
def function(mark, s=s):
    m=re.search(re.escape(mark)+r"[^;{}]*\{",s)
    assert m,mark
    a=s.index("{",m.start());b=a+1;depth=1
    while depth:
        depth+=(s[b]=="{")-(s[b]=="}");b+=1
    return s[m.start():b]
assert "if(sv.potions < 9 && DM(ox, oy) == D_POTION)" in s
assert "dg_pit_kind(pit_wave, i, rndn(12))" in s
assert "for(i = 0; i < NMON; i++) if(mon[i].alive) return 0;" in s
assert "if(HIT(J_A) && !(k & J_SELECT) && !atk)" in s
assert "(HIT(J_SELECT) && (k & J_A)) || (HIT(J_A) && (k & J_SELECT))" in s
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int8_t INT8;
typedef int16_t INT16;
#define DW 32
#define NMON 10
#define BANKED
#define DG_BASE_FAMILY(k) ((k)&3)
#define LUCKY(x) (x)
enum {K_SPIDER,K_BEETLE,K_WASP,K_SLIME,K_CRAWLER,K_GOLEM,K_MOTH,K_SPORE,K_SHADE,K_STINGER,K_CRAB,K_REAPER,K_QUEEN};
enum {D_FLOOR,D_FLOOR2,D_WALL,D_FACE,D_DOWN,D_UP,D_CHEST,D_OPEN,D_CAGE,D_FREED,D_POTION};
enum {SFX_HIT,SFX_FIRE,SFX_DIE,SFX_COIN,SFX_FANFARE};
typedef struct {UINT16 x,y;UINT8 kind,hp,cool,flash,frame,alive;INT8 kx,ky;UINT8 st,t;INT8 lx,ly;} Mon;
static Mon mon[NMON];
static UINT16 bx,by;
static UINT8 face,stop,atk,depth,floors,hell,pit,boss_clue_seen,kind;
static struct {UINT32 gold;UINT8 zone,star,cores,potions,friends;} sv;
static UINT8 dmap[DW*DW];
#define DM(x,y) dmap[(y)*DW+(x)]
static unsigned paid,parents,children,queen_kills,xp,bad_pay;
static UINT8 bite_power(void){return 30;}
static UINT8 rndn(UINT8 n){(void)n;return 1;}
static UINT8 blocked(UINT16 x,UINT16 y,UINT8 r){(void)r;return x>=512||y>=512;}
static UINT8 world_freed(UINT8 star){(void)star;return 0;}
static void set_mt(UINT8 x,UINT8 y,UINT8 t){DM(x,y)=t;}
static void boss_evidence(UINT8 c){(void)c;}
static UINT16 loot_clover(UINT16 n){(void)n;return 0;}
static UINT8 loot_chest(UINT8 d,char*b){(void)d;(void)b;return 0;}
static void dlg_say(const char*a,const char*b){(void)a;(void)b;}
static void fx_add(UINT16 x,UINT16 y,UINT8 a,UINT8 b){(void)x;(void)y;(void)a;(void)b;}
static void sfx(int n){(void)n;}
static void shake(int n){(void)n;}
static void gain_xp(unsigned n){xp+=n;}
static void bst_kill(UINT8 k,UINT8 w,UINT8 d){(void)w;(void)d;if(k==K_QUEEN)queen_kills++;}
static char *scat(char*b,const char*t){strcpy(b,t);return b+strlen(t);}
static char *u16s(char*b,UINT16 n){sprintf(b,"%u",n);return b+strlen(b);}
static void kill_pay(Mon*m){paid++;if(m->alive||m->hp)bad_pay++;if(m->st==9)children++;else parents++;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"GBC combat line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
cases=r"""
static void reset(void){
    memset(mon,0,sizeof mon);memset(dmap,D_FLOOR,sizeof dmap);memset(&sv,0,sizeof sv);
    paid=parents=children=queen_kills=xp=bad_pay=0;bx=96;by=100;face=0;depth=3;floors=5;
}
static void parent_at(int i,int k){mon[i]=(Mon){.x=112,.y=100,.kind=k,.hp=5,.alive=1};}
int main(void){
    unsigned seen=0;
    for(int wave=1;wave<=10;wave++){
        int a=dg_pit_kind(wave,0,0),b=dg_pit_kind(wave,1,0);
        CHECK(a!=b);
        for(int slot=0;slot<8;slot++)for(int roll=0;roll<256;roll++){
            int k=dg_pit_kind(wave,slot,roll);
            CHECK(k>=K_SPIDER&&k<K_QUEEN);
            if(wave<=2)CHECK(k<=K_SLIME);
            if(slot==0)CHECK(k==a);
            if(slot==1)CHECK(k==b);
            seen|=1u<<k;
        }
    }
    CHECK(seen==(1u<<K_QUEEN)-1);
    CHECK(dg_pit_kind(0,0,0)==K_SPIDER&&dg_pit_kind(11,0,0)==K_SPIDER);
    puts("GBC pit: 20,480 roster checks across ten waves and twelve species PASS");
    for(int k=K_SLIME;k<=K_SPORE;k+=K_SPORE-K_SLIME){
        for(int slot=0;slot<NMON-1;slot++){
            reset();parent_at(slot,k);do_bite();
            CHECK(paid==1&&parents==1&&!children&&!bad_pay);
            unsigned n=0,west=0,east=0;
            for(int j=0;j<NMON;j++)if(mon[j].alive){
                CHECK(mon[j].kind==k&&mon[j].st==9&&mon[j].hp==5);
                CHECK(mon[j].y==100);
                west+=mon[j].x==102;east+=mon[j].x==122;n++;
            }
            CHECK(n==2&&west==1&&east==1);
            do_bite();CHECK(paid==3&&parents==1&&children==2&&!bad_pay);
            for(int j=0;j<NMON;j++)CHECK(!mon[j].alive);
        }
    }
    // Dense floor: spawn exactly the one child that fits, leave the queen.
    reset();for(int j=0;j<NMON;j++)mon[j]=(Mon){.x=450,.y=450,.kind=K_SPIDER,.hp=9,.alive=1};
    mon[NMON-1].kind=K_QUEEN;parent_at(4,K_SLIME);
    do_bite();CHECK(paid==1&&!bad_pay&&mon[4].alive&&mon[4].st==9&&mon[4].x==102);
    CHECK(mon[NMON-1].alive&&mon[NMON-1].kind==K_QUEEN&&mon[NMON-1].hp==9);
    reset();parent_at(NMON-1,K_QUEEN);do_bite();
    CHECK(queen_kills==1&&!paid&&!mon[NMON-1].alive&&sv.gold==200&&xp==40);
    puts("GBC combat: 18 parent splits, child placement, separate bites, crowded floor, queen rewards and input/potion guards PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-gbc-combat-") as td:
    c=Path(td)/"combat.c";exe=Path(td)/"combat"
    c.write_text(prefix+function("static void mon_spawn(")+"\n"+function("static void do_bite(")+"\n"+function("UINT8 dg_pit_kind(",extra)+"\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-fsanitize=address,undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
