/* Host-side test of the save system and menus: fake 32 KB SRAM, scripted buttons. Run: make test */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <gb/gb.h>
#include "save.h"
#include "menus.h"
#include "input.h"
UINT8 stub_div, stub_vbk, _cpu; int stub_bank;
unsigned char sram_buf[4*8192];
/* ---- stubs */
static int cx, cy; static char scr[18][21];
static const UINT8 *script; static int sp, phase;
UINT8 joypad(void){ if(!script) return 0; UINT8 k = script[sp]; if(k==0xFF) { fprintf(stderr,"script ended\n"); exit(2);} return phase ? k : 0; }
void wait_vbl_done(void){ if(phase){ phase=0; sp++; } else phase=1; /* press for one poll, release for one */ }
void gotoxy(UINT8 x, UINT8 y){ cx=x; cy=y; }
void cls(void){ memset(scr,' ',sizeof scr); for(int i=0;i<18;i++) scr[i][20]=0; }
int printf(const char *f, ...){ va_list a; va_start(a,f); char b[64]; vsnprintf(b,64,f,a); va_end(a); for(char*p=b;*p;p++){ if(cy<18&&cx<20) scr[cy][cx]=*p; cx++; } return 0; }
void add_VBL(void(*f)(void)){} void set_interrupts(UINT8 x){}
static void dump(const char*t){ fprintf(stdout,"--- %s\n",t); for(int i=0;i<18;i++){ char l[21]; memcpy(l,scr[i],20); l[20]=0; int e=19; while(e>=0&&l[e]==' ') l[e--]=0; fprintf(stdout,"|%s\n",l);} }
#define CHECK(c) do{ if(!(c)){ fprintf(stdout,"FAIL line %d: %s\n",__LINE__,#c); fails++; } }while(0)
int fails;
int main(void){
    extern UINT8 keys, keys_old; (void)keys; (void)keys_old;
    cls();
    /* fake SRAM erased to 0xFF like a fresh cart */
    memset(sram_buf,0xFF,sizeof sram_buf);
    save_init();
    CHECK(opt.music==2 && opt.diff==1 && opt.autosave==1);
    CHECK(first_good_slot()==255);
    new_game(1, 0xDEADBEEF);
    CHECK(first_good_slot()==1); CHECK(last_slot==1);
    Save t; CHECK(slot_peek(1,&t)); CHECK(t.seed==0xDEADBEEF && t.level==1 && t.saves==1 && !strcmp(t.name,"BUG"));
    CHECK(!slot_peek(0,&t)); CHECK(!slot_peek(2,&t));
    sv.level=7; sv.gold=123456; slot_write(1); CHECK(slot_read(1)); CHECK(sv.level==7 && sv.gold==123456 && sv.saves==2);
    /* corruption is detected */
    sram_buf[8192+10]^=1; CHECK(!slot_peek(1,&t)); sram_buf[8192+10]^=1; CHECK(slot_peek(1,&t));
    /* settings persist across a "reboot" and clamp bad values */
    opt.music=0; opt.diff=3; opt.xpm=2; settings_save();
    opt.music=2; opt.diff=1; opt.xpm=0; last_slot=0;
    save_init(); CHECK(opt.music==0 && opt.diff==3 && opt.xpm==2 && last_slot==1);
    sram_buf[3*8192+6]=9; /* corrupt settings -> defaults */ save_init(); CHECK(opt.music==2 && opt.diff==1);
    /* second slot, erase logic */
    new_game(2, 5); CHECK(last_slot==2); slot_erase(2); CHECK(!slot_peek(2,&t)); CHECK(first_good_slot()==1);
    /* formatting helpers */
    char b[32]; fmt_time(b,3725); CHECK(!strcmp(b,"1:02:05")); fmt_time(b,0); CHECK(!strcmp(b,"0:00:00"));
    fmt_hex32(b,0xDEADBEEF); CHECK(!strcmp(b,"DEADBEEF")); u16s(b,0); CHECK(!strcmp(b,"0")); u16s(b,65535); CHECK(!strcmp(b,"65535"));
    /* ---- slot picker: NEW on an empty slot: DOWN, A  -> slot 0? (starts on slot 0, empty) */
    static const UINT8 s1[] = { J_A, 0xFF };
    script=s1; sp=0; phase=0; cls(); int r=slot_picker(1); CHECK(r==0); dump("picker NEW after choosing empty slot 1");
    /* NEW on used slot 1 asks to overwrite; B declines, then B backs out */
    static const UINT8 s2[] = { J_DOWN, J_A, J_B, J_B, 0xFF };
    script=s2; sp=0; phase=0; cls(); r=slot_picker(1); CHECK(r==-1);
    /* overwrite confirm with A -> returns 1 */
    static const UINT8 s3[] = { J_DOWN, J_A, J_A, 0xFF };
    script=s3; sp=0; phase=0; cls(); r=slot_picker(1); CHECK(r==1);
    /* LOAD ignores empty slots, selects used one (starts on last used = 1) */
    static const UINT8 s4[] = { J_UP, J_A, J_DOWN, J_A, 0xFF };
    script=s4; sp=0; phase=0; cls(); r=slot_picker(0); CHECK(r==1); dump("picker LOAD");
    /* erase with START + A */
    static const UINT8 s5[] = { J_START, J_A, J_B, 0xFF };
    script=s5; sp=0; phase=0; cls(); r=slot_picker(0); CHECK(r==-1); CHECK(!slot_peek(1,&t));
    /* options: toggle BABY (right), cycle DIFFICULTY left twice, go to the bottom, B to save */
    static const UINT8 o1[] = { J_RIGHT, J_DOWN, J_LEFT, J_LEFT, J_UP, J_UP, J_B, 0xFF };
    save_init(); script=o1; sp=0; phase=0; cls(); options_screen();
    CHECK(opt.baby==1); CHECK(opt.diff==4 /* 1->0->4 */); CHECK(opt.arrow==1); dump("options after edits (cursor wrapped to bottom)");
    memset(&opt,0,sizeof opt); save_init(); CHECK(opt.baby==1 && opt.diff==4);
    fprintf(stdout, "%s\n", fails ? "FAILED" : "ALL OK");
    return fails;
}
