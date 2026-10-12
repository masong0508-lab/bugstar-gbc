/* Host-side test of the save system and menus: fake 32 KB SRAM, scripted buttons. Run: make test */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <gb/gb.h>
#include <gb/cgb.h>
#include "save.h"
#include "menus.h"
#include "input.h"
#include "text.h"
#include "hud.h"
UINT8 stub_div, stub_vbk, _cpu, stub_win, stub_wy; int stub_bank;
unsigned char sram_buf[4*8192];
/* ---- stubs */
/* fake BG map (bank 0) and attribute map (bank 1): the real text engine draws into these */
static UINT8 tmap[32][32], amap[32][32]; UINT8 pal_loaded; int font_loaded_n;
static UINT8 wmap[32][32], wamap[32][32];
static void wst_set(UINT8 x, UINT8 y, UINT8 t){ if(stub_vbk) wamap[y & 31][x & 31] = t; else wmap[y & 31][x & 31] = t; }
void set_win_tiles(UINT8 x, UINT8 y, UINT8 w, UINT8 h, const UINT8 *t){ for(UINT8 j = 0; j < h; j++) for(UINT8 i = 0; i < w; i++) wst_set(x + i, y + j, t[j * w + i]); }
void fill_win_rect(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 t){ for(UINT8 j = 0; j < h; j++) for(UINT8 i = 0; i < w; i++) wst_set(x + i, y + j, t); }
void move_win(UINT8 x, UINT8 y){ (void)x; stub_wy = y; }
static int wrow_is(int y, int x, const char *want){ for(int i=0; want[i]; i++) if(wmap[y][x+i] != (UINT8)want[i]) return 0; return 1; }
static void st_set(UINT8 x, UINT8 y, UINT8 t){ if(stub_vbk) amap[y & 31][x & 31] = t; else tmap[y & 31][x & 31] = t; }
void set_bkg_data(UINT8 f, UINT8 n, const UINT8 *d){ (void)f; (void)d; font_loaded_n = n; }
void set_bkg_palette(UINT8 f, UINT8 n, const palette_color_t *p){ (void)f; (void)p; pal_loaded = n; }
void set_bkg_tiles(UINT8 x, UINT8 y, UINT8 w, UINT8 h, const UINT8 *t){ for(UINT8 j = 0; j < h; j++) for(UINT8 i = 0; i < w; i++) st_set(x + i, y + j, t[j * w + i]); }
void fill_bkg_rect(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 t){ for(UINT8 j = 0; j < h; j++) for(UINT8 i = 0; i < w; i++) st_set(x + i, y + j, t); }
static char glyph(UINT8 t){ switch(t){ case 0: return ' '; case T_CURSOR: return '>'; case T_ARROW_DN: return 'v'; case T_ARROW_UP: return '^'; default: return t >= 32 && t < 127 ? (char)t : '#'; } }
static const UINT8 *script; static int sp, phase;
UINT8 joypad(void){ if(!script) return 0; UINT8 k = script[sp]; if(k==0xFF) { fprintf(stderr,"script ended\n"); exit(2);} return phase ? k : 0; }
void wait_vbl_done(void){ if(phase){ phase=0; sp++; } else phase=1; /* press for one poll, release for one */ }
void gotoxy(UINT8 x, UINT8 y){ (void)x; (void)y; }
void cls(void){ txt_cls(); }
void add_VBL(void(*f)(void)){} void set_interrupts(UINT8 x){}
static void dump(const char*t){ fprintf(stdout,"--- %s\n",t); for(int i=0;i<18;i++){ char l[21]; for(int c=0;c<20;c++) l[c]=glyph(tmap[i][c]); l[20]=0; int e=19; while(e>=0&&l[e]==' ') l[e--]=0; fprintf(stdout,"|%s\n",l);} }
static int row_is(int y, int x, const char *want){ for(int i=0; want[i]; i++) if(tmap[y][x+i] != (UINT8)want[i]) return 0; return 1; }
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
    /* ---- font data: tile number == ASCII, every drawn glyph has ink, aliases and specials are right */
    CHECK(sizeof bs_font == 128 * 16);
    for(int t = 0; t < 128; t++){
        int ink = 0, shadow = 0;
        for(int r = 0; r < 8; r++){ UINT8 lo = bs_font[t*16+r*2], hi = bs_font[t*16+r*2+1]; ink |= (lo & hi); shadow |= (lo & ~hi); }
        if(t == 0 || t == ' ') CHECK(!ink && !shadow);
        if((t >= 33 && t <= 90) || (t >= 123 && t <= 126) || t == '`') CHECK(ink);
        if(t >= 'A' && t <= 'Z') { CHECK(shadow); CHECK(!memcmp(bs_font + t*16, bs_font + (t + 32)*16, 16)); }
    }
    CHECK(bs_font[T_BOX_TL*16+2] == 0x3F); CHECK(T_BAR8 - T_BAR0 == 8);
    CHECK(S_CURSOR[0] == T_CURSOR && S_CURSOR[1] == 0 && S_HEART[0] == T_HEART);
    /* ---- text engine */
    txt_init(); CHECK(font_loaded_n == FONT_NTILES && pal_loaded == 4);
    txt_put(2, 3, "AB"); CHECK(tmap[3][2] == 'A' && tmap[3][3] == 'B' && tmap[3][4] == T_BLANK);
    txt_line(1, 4, "HELLO", 8); CHECK(row_is(4, 1, "HELLO") && tmap[4][6] == T_BLANK && tmap[4][8] == T_BLANK);
    txt_line(0, 5, "TOOLONGTEXT", 4); CHECK(row_is(5, 0, "TOOL") && tmap[5][4] == T_BLANK);
    txt_put(0, 6, "a" S_HEART "z"); CHECK(tmap[6][0] == 'a' && tmap[6][1] == T_HEART && tmap[6][2] == 'z');
    txt_box(1, 7, 6, 4);
    CHECK(tmap[7][1] == T_BOX_TL && tmap[7][3] == T_BOX_T && tmap[7][6] == T_BOX_TR);
    CHECK(tmap[8][1] == T_BOX_L && tmap[8][3] == T_BLANK && tmap[8][6] == T_BOX_R);
    CHECK(tmap[10][1] == T_BOX_BL && tmap[10][4] == T_BOX_B && tmap[10][6] == T_BOX_BR && tmap[7][0] == T_BLANK && tmap[7][7] == T_BLANK);
    /* bar: 6 tiles wide = 4 middle tiles = 32 px; 50% = 16 px = two full tiles then two empty */
    txt_bar(0, 12, 6, 50, 100);
    CHECK(tmap[12][0] == T_BAR_L && tmap[12][1] == T_BAR8 && tmap[12][2] == T_BAR8 && tmap[12][3] == T_BAR0 && tmap[12][4] == T_BAR0 && tmap[12][5] == T_BAR_R);
    txt_bar(0, 13, 6, 3, 32 * 1);                       /* 3/32 of 32 px = 3 px */
    CHECK(tmap[13][1] == T_BAR0 + 3 && tmap[13][2] == T_BAR0);
    txt_bar(0, 14, 6, 999, 100); CHECK(tmap[14][4] == T_BAR8);          /* over max clamps to full */
    txt_bar(0, 15, 6, 5, 0);     CHECK(tmap[15][1] == T_BAR0);          /* max 0 is empty, no divide by zero */
    txt_bar(0, 16, 4, 65535, 65535); CHECK(tmap[16][1] == T_BAR8 && tmap[16][2] == T_BAR8);  /* 16-bit values do not overflow */
    txt_bar(0, 17, 4, 32768, 65535); CHECK(tmap[17][1] == T_BAR8 && tmap[17][2] == T_BAR0);
    txt_color(0, 3, 3, 1, TXT_GOLD); CHECK(amap[3][0] == 1 && amap[3][2] == 1 && amap[3][3] == 0 && stub_vbk == 0);
    txt_cls(); CHECK(tmap[3][2] == T_BLANK && amap[3][0] == 0);
    /* ---- u32s */
    u32s(b, 0); CHECK(!strcmp(b, "0")); u32s(b, 4294967295UL); CHECK(!strcmp(b, "4294967295")); u32s(b, 1000000UL); CHECK(!strcmp(b, "1000000"));
    /* ---- window HUD */
    txt_cls(); txt_put(0, 16, "BG ROW");
    memset(&sv, 0, sizeof sv); sv.level = 1; sv.hp = 10; sv.mp = 5; sv.gold = 0;
    hud_init();
    CHECK(stub_win == 1 && stub_wy == HUD_Y && HUD_Y == 128);
    CHECK(wmap[0][0] == T_HEART && wmap[0][1] == T_BAR_L && wmap[0][2] == T_BAR8 && wmap[0][5] == T_BAR8 && wmap[0][6] == T_BAR_R);
    CHECK(wrow_is(0, 7, "10") && wmap[0][9] == T_BLANK && wrow_is(0, 11, "LV 1") && wmap[0][15] == T_BLANK);
    CHECK(wmap[1][0] == T_COIN && wrow_is(1, 1, "0") && wmap[1][2] == T_BLANK && wmap[1][10] == T_STAR && wamap[1][10] == TXT_GOLD);
    CHECK(wmap[1][11] == T_BAR_L && wmap[1][12] == T_BAR8 && wmap[1][16] == T_BAR_R && wrow_is(1, 17, "5"));
    CHECK(wamap[0][3] == TXT_WHITE);
    CHECK(row_is(16, 0, "BG ROW"));                              /* HUD never touches the background map */
    txt_put(0, 17, "AFTER"); CHECK(row_is(17, 0, "AFTER") && wmap[17][0] == 0);   /* target went back to BG */
    /* low HP turns the left half red, healing turns it back */
    sv.hp = 2; hud_update(); CHECK(wamap[0][0] == TXT_RED && wamap[0][9] == TXT_RED && wamap[0][10] == 0 && wrow_is(0, 7, "2") && wmap[0][7 + 1] == T_BLANK);
    sv.hp = 3; hud_update(); CHECK(wamap[0][0] == TXT_WHITE);    /* 3*4 = 12 >= 10: exactly past the 25% line */
    sv.hp = 2; hud_update(); CHECK(wamap[0][0] == TXT_RED);
    sv.hp = 10; hud_update(); CHECK(wamap[0][0] == TXT_WHITE && wrow_is(0, 7, "10"));
    /* level 99: max hp 500; 250 hp is half a bar = 2 full tiles of the 4 middle ones */
    sv.level = 99; sv.hp = 250; sv.mp = 299; hud_update();
    CHECK(wrow_is(0, 11, "LV 99") && hud_hpmax(99) == 500 && hud_mpmax(99) == 299 && hud_hpmax(1) == 10 && hud_mpmax(1) == 5);
    CHECK(wmap[0][2] == T_BAR8 && wmap[0][3] == T_BAR8 && wmap[0][4] == T_BAR0 && wmap[0][5] == T_BAR0 && wrow_is(0, 7, "250"));
    CHECK(wmap[1][12] == T_BAR8 && wmap[1][15] == T_BAR8 && wrow_is(1, 17, "299"));
    /* gold: 9 digits max, clamps above that */
    sv.gold = 123456789UL; hud_update(); CHECK(wrow_is(1, 1, "123456789"));
    sv.gold = 4000000000UL; hud_update(); CHECK(wrow_is(1, 1, "999999999"));
    sv.gold = 7; hud_update(); CHECK(wrow_is(1, 1, "7") && wmap[1][2] == T_BLANK && wmap[1][9] == T_BLANK);
    /* unchanged stats redraw nothing: poke a tile, update, it stays poked */
    wmap[0][4] = 99; hud_update(); CHECK(wmap[0][4] == 99);
    sv.hp = 251; hud_update(); CHECK(wmap[0][4] != 99);
    hud_pos(144); CHECK(stub_wy == 144); hud_hide(); CHECK(stub_win == 0); hud_show(); CHECK(stub_win == 1 && stub_wy == HUD_Y);
    fprintf(stdout, "%s\n", fails ? "FAILED" : "ALL OK");
    return fails;
}
