#!/usr/bin/env python3
"""Execute the actual GBC mining timing game with simulated buttons and VRAM writes."""
from pathlib import Path
import subprocess
import tempfile

s=(Path(__file__).resolve().parents[1]/"src/xtra.c").read_text()
a=s.index("static UINT8 mine_rank(")
b=s.index("/* ---------------------------------------------------------------- the station LOUNGE */",a)
code=s[a:b]
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
#define BANKED
#define J_A 1
#define J_B 2
#define J_START 4
#define HIT(k) (buttons&(k))
#define TXT_WHITE 0
#define TXT_GOLD 1
#define TXT_RED 2
#define TXT_GREY 3
#define SFX_COIN 0
#define SFX_HURT 1
#define SFX_FANFARE 2
#define SFX_OK 3
#define WP_ORE 3
static struct { UINT32 gold; UINT8 star; } sv;
static struct {UINT16 ore;UINT8 best;} xs;
static unsigned frames,buttons,mode,save_count,wait_count,bar_writes,bar_cells,bar_colors,marker,target,active,swing;
static char bar_display[19];
static int hit_sent,early_sent,result_hold;
static void xs_load(void){}
static void xs_save(void){save_count++;}
static UINT8 xrnd(void){return 0;} // target=3, center=4
static void screen(const char*s){(void)s;active=0;}
static void gold_line(void){}
static void wait_a(void){wait_count++;}
static void sfx(int x){(void)x;}
static UINT8 weird_of(UINT8 star){return star;}
static UINT8 weird_pct(UINT8 w,UINT8 what){(void)w;(void)what;return 0;}
static char* scat(char*d,const char*s){strcpy(d,s);return d+strlen(s);}
static char* u16s(char*d,UINT16 n){sprintf(d,"%u",n);return d+strlen(d);}
static void wait_vbl_done(void){if(++frames>10000){fputs("mining timeout\n",stderr);exit(2);}}
static void txt_line(UINT8 x,UINT8 y,const char*s,UINT8 w){
    if(y==3 && strncmp(s,"SWING ",6)==0){swing++;hit_sent=0;result_hold=0;}
    if(y==10 && *s)result_hold=1;
    if(y!=8)return;
    if(x<1 || x+w>19){fputs("bar outside panel\n",stderr);exit(2);}
    bar_writes++;bar_cells+=w;
    if(w==18){memcpy(bar_display,s,18);marker=0;active=1;}
    else if(w==1){bar_display[x-1]=*s;if(*s=='V')marker=x-1;}
    else exit(3);
}
static void txt_color(UINT8 x,UINT8 y,UINT8 w,UINT8 h,UINT8 pal){
    (void)x;(void)w;(void)h;(void)pal;if(y==8)bar_colors++;
}
static void input_poll(void){
    buttons=0;
    if(frames==1){buttons=mode==0?J_B:J_A;return;}
    if(!active)return;
    if(mode==1 && swing==1){buttons=J_B;return;}
    if(mode==2 && swing==3 && !result_hold && !early_sent){buttons=J_B;early_sent=1;return;}
    if(mode==4 && swing==2 && result_hold && !early_sent){buttons=J_START;early_sent=1;return;}
    if(!hit_sent && marker==4){buttons=J_A;hit_sent=1;}
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mining regression line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
tests=r"""
static void reset(unsigned m){
    mode=m;frames=buttons=save_count=wait_count=bar_writes=bar_cells=bar_colors=marker=target=active=swing=0;
    hit_sent=early_sent=result_hold=0;sv.gold=100;xs.ore=65534;xs.best=0;
}
int main(void){
    for(int t=3;t<=12;t++)for(int m=0;m<18;m++){
        int d=abs(m-(t+1));
        CHECK(mine_rank(m,t)==(d==0?4:d==1?3:d==2?2:d<=4?1:0));
    }
    UINT8 streak=0,best=0;
    CHECK(mine_swing_pay(4,&streak,&best)==40 && streak==1 && best==1);
    CHECK(mine_swing_pay(3,&streak,&best)==26 && streak==2 && best==2);
    CHECK(mine_swing_pay(2,&streak,&best)==10 && !streak && best==2);
    CHECK(mine_total_pay(100,4,4,25)==125);
    CHECK(mine_total_pay(100,8,4,25)==250);
    reset(0);xtra_mine();CHECK(!swing && !save_count && sv.gold==100);
    reset(1);xtra_mine();CHECK(swing==1 && !save_count && sv.gold==100);
    reset(2);xtra_mine();CHECK(swing==3 && save_count==1 && sv.gold==186 && xs.ore==0 && wait_count==1);
    reset(3);xtra_mine();
    CHECK(swing==8 && save_count==1 && wait_count==1 && sv.gold==625 && xs.ore==6);
    CHECK(bar_colors==16);
    CHECK(bar_cells==8*(18+4*2)); // full initial bar plus two cells per actual movement
    CHECK(bar_writes==8*(1+4*2));
    CHECK(frames>bar_writes); // stationary frames and result holds still poll input
    reset(4);xtra_mine();CHECK(swing==2 && save_count==1 && sv.gold==186 && xs.ore==0);
    puts("GBC mining: visible marker, exact reward, early cash-out, cancel, counter wrap and reduced VRAM writes PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"mining.c";exe=Path(td)/"mining"
    c.write_text(prefix+"\n"+code+"\n"+tests)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined,address","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
