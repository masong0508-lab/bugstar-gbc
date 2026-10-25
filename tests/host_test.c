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
#include "dialog.h"
#include "sysmenu.h"
#include "bug.h"
#include "bugmake.h"
UINT8 bugmake_run(BugLook *look, char *name, UINT8 first){ return 0; }
void skills_run(void){ }                                          /* (and the skills page) */   /* (the creator needs the Game Boy screen: the host tests only link the menu) */
UINT8 stub_div, stub_vbk, _cpu, stub_win, stub_wy, stub_wx; int stub_bank;
unsigned char sram_buf[4*8192];
/* ---- stubs */
/* fake BG map (bank 0) and attribute map (bank 1): the real text engine draws into these */
static UINT8 tmap[32][32], amap[32][32]; UINT8 pal_loaded; int font_loaded_n;
static UINT8 wmap[32][32], wamap[32][32];
static void wst_set(UINT8 x, UINT8 y, UINT8 t){ if(stub_vbk) wamap[y & 31][x & 31] = t; else wmap[y & 31][x & 31] = t; }
void set_win_tiles(UINT8 x, UINT8 y, UINT8 w, UINT8 h, const UINT8 *t){ for(UINT8 j = 0; j < h; j++) for(UINT8 i = 0; i < w; i++) wst_set(x + i, y + j, t[j * w + i]); }
void fill_win_rect(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 t){ for(UINT8 j = 0; j < h; j++) for(UINT8 i = 0; i < w; i++) wst_set(x + i, y + j, t); }
void move_win(UINT8 x, UINT8 y){ stub_wx = x; stub_wy = y; }
static int wrow_is(int y, int x, const char *want){ for(int i=0; want[i]; i++) if(wmap[y][x+i] != (UINT8)want[i]) return 0; return 1; }
static void st_set(UINT8 x, UINT8 y, UINT8 t){ if(stub_vbk) amap[y & 31][x & 31] = t; else tmap[y & 31][x & 31] = t; }
void set_bkg_data(UINT8 f, UINT8 n, const UINT8 *d){ (void)f; (void)d; font_loaded_n = n; }
void set_bkg_palette(UINT8 f, UINT8 n, const palette_color_t *p){ (void)f; (void)p; pal_loaded = n; }
void set_bkg_tiles(UINT8 x, UINT8 y, UINT8 w, UINT8 h, const UINT8 *t){ for(UINT8 j = 0; j < h; j++) for(UINT8 i = 0; i < w; i++) st_set(x + i, y + j, t[j * w + i]); }
void fill_bkg_rect(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 t){ for(UINT8 j = 0; j < h; j++) for(UINT8 i = 0; i < w; i++) st_set(x + i, y + j, t); }
static char glyph(UINT8 t){ switch(t){ case 0: return ' '; case T_CURSOR: return '>'; case T_ARROW_DN: return 'v'; case T_ARROW_UP: return '^'; default: return t >= 32 && t < 127 ? (char)t : '#'; } }
static const UINT8 *script; static int sp, phase;
static UINT8 (*padfn)(int); static int frames;
UINT8 joypad(void){ if(padfn) return padfn(frames); if(!script) return 0; UINT8 k = script[sp]; if(k==0xFF) { fprintf(stderr,"script ended\n"); exit(2);} return phase ? k : 0; }
void wait_vbl_done(void){ frames++; if(phase){ phase=0; sp++; } else phase=1; /* press for one poll, release for one */ }
void gotoxy(UINT8 x, UINT8 y){ (void)x; (void)y; }
void cls(void){ txt_cls(); }
void add_VBL(void(*f)(void)){} void set_interrupts(UINT8 x){}
void sfx(UINT8 n){ (void)n; } void music(UINT8 n){ (void)n; }
static void dump(const char*t){ fprintf(stdout,"--- %s\n",t); for(int i=0;i<18;i++){ char l[21]; for(int c=0;c<20;c++) l[c]=glyph(tmap[i][c]); l[20]=0; int e=19; while(e>=0&&l[e]==' ') l[e--]=0; fprintf(stdout,"|%s\n",l);} }
static int row_is(int y, int x, const char *want){ for(int i=0; want[i]; i++) if(tmap[y][x+i] != (UINT8)want[i]) return 0; return 1; }
#define CHECK(c) do{ if(!(c)){ fprintf(stdout,"FAIL line %d: %s\n",__LINE__,#c); fails++; } }while(0)
int fails;
/* ---- dialogue test pad: presses A whenever the "more" arrow is showing, snapshotting the window first */
static int d_snaps, d_last, d_blips, d_rush_at = -1, d_start;
static UINT8 d_snap[4][6][20], d_snapa[4][6][20];
static UINT8 pad_dlg(int f){
    UINT8 r = 0;
    if(d_rush_at >= 0 && f - d_start == d_rush_at) return J_A;
    if(wmap[5][18] == T_ARROW_DN && stub_wy == DLG_Y && !d_last){
        if(d_snaps < 4){ for(int y = 0; y < 6; y++){ memcpy(d_snap[d_snaps][y], wmap[y], 20); memcpy(d_snapa[d_snaps][y], wamap[y], 20); } }
        d_snaps++; r = J_A;
    }
    d_last = r ? 1 : 0; return r;
}
static void d_blip(void){ d_blips++; }
static int snap_is(int pg, int y, const char *want){ for(int i = 0; want[i]; i++) if(d_snap[pg][y][1 + i] != (UINT8)want[i]) return 0; return strlen(want) >= 18 || d_snap[pg][y][1 + (int)strlen(want)] == T_BLANK; }   /* a full 18-column line runs into the frame */
/* ---- SYSTEM menu test pad: each step is one press then one release; every poll snapshots the window and state */
static const UINT8 *sq; static int sq_n, sq_calls, sq_win[40], sq_wx[40]; static UINT8 sq_snap[40][10][12];
static int redraws;
static UINT8 pad_seq(int f){
    (void)f; int i = sq_n / 2; UINT8 k = sq[i];
    if(k == 0xFF){ fprintf(stderr, "sysmenu script ended\n"); exit(2); }
    if(sq_n < 40){ for(int y = 0; y < 10; y++) memcpy(sq_snap[sq_n][y], wmap[y], 12); sq_win[sq_n] = stub_win; sq_wx[sq_n] = stub_wx; }
    return (sq_n++ & 1) ? 0 : k;
}
static void sm_repaint(void){ redraws++; }
static int sm_run(const UINT8 *seq){ keys = keys_old = 0; sq = seq; sq_n = 0; redraws = 0; padfn = pad_seq; int r = sysmenu_run(sm_repaint); padfn = 0; return r; }
static int sq_is(int n, int y, int x, const char *want){ for(int i = 0; want[i]; i++) if(sq_snap[n][y][x + i] != (UINT8)want[i]) return 0; return 1; }
static int d_run(const char *name, const char *text, UINT8 spd){ keys = keys_old = 0; d_snaps = 0; d_last = 0; d_blips = 0; padfn = pad_dlg; d_start = frames; dlg_set_speed(spd); dlg_say(name, text); padfn = 0; return frames - d_start; }
unsigned char SCX_REG, SCY_REG, LCDC_REG;
int main(void){
    extern UINT8 keys, keys_old; (void)keys; (void)keys_old;
    cls();
    /* fake SRAM erased to 0xFF like a fresh cart */
    memset(sram_buf,0xFF,sizeof sram_buf);
    save_init();
    CHECK(opt.music==2 && opt.diff==1 && opt.autosave==1);
    CHECK(first_good_slot()==255);
    new_game(1, 0xDEADBEEF, 0, 0);
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
    new_game(2, 5, 0, 0); CHECK(last_slot==2); slot_erase(2); CHECK(!slot_peek(2,&t)); CHECK(first_good_slot()==1);
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
    /* ---- dialogue: word wrap */
    { const char *p; char l[20]; UINT8 n;
      p = "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG";
      n = dlg_next_line(&p, l); CHECK(n == 15 && !strcmp(l, "THE QUICK BROWN"));
      n = dlg_next_line(&p, l); CHECK(n == 18 && !strcmp(l, "FOX JUMPS OVER THE"));
      n = dlg_next_line(&p, l); CHECK(n == 8 && !strcmp(l, "LAZY DOG"));
      n = dlg_next_line(&p, l); CHECK(n == 255);
      p = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      n = dlg_next_line(&p, l); CHECK(n == 18 && !strcmp(l, "ABCDEFGHIJKLMNOPQR"));
      n = dlg_next_line(&p, l); CHECK(n == 8 && !strcmp(l, "STUVWXYZ"));
      p = "A\nB"; n = dlg_next_line(&p, l); CHECK(n == 1 && l[0] == 'A'); n = dlg_next_line(&p, l); CHECK(n == 1 && l[0] == 'B'); CHECK(dlg_next_line(&p, l) == 255);
      p = "A\n\nB"; dlg_next_line(&p, l); n = dlg_next_line(&p, l); CHECK(n == 0 && !l[0]); n = dlg_next_line(&p, l); CHECK(n == 1 && l[0] == 'B');
      p = "  HI  \nYO"; n = dlg_next_line(&p, l); CHECK(n == 2 && !strcmp(l, "HI")); n = dlg_next_line(&p, l); CHECK(n == 2 && !strcmp(l, "YO"));
      p = ""; CHECK(dlg_next_line(&p, l) == 255);
      p = "EXACTLY EIGHTEEN!!"; n = dlg_next_line(&p, l); CHECK(n == 18); CHECK(dlg_next_line(&p, l) == 255);
    }
    /* ---- dialogue: box, name, two pages, HUD slides away and back */
    memset(&sv, 0, sizeof sv); sv.level = 1; sv.hp = 10; sv.mp = 5; hud_init();
    { dlg_set_blip(d_blip); int fr = d_run("BUG", "ONE\nTWO\nTHREE\nFOUR", 1);
      CHECK(d_snaps == 2); (void)fr;
      CHECK(d_snap[0][0][0] == T_BOX_TL && d_snap[0][0][19] == T_BOX_TR && d_snap[0][5][0] == T_BOX_BL && d_snap[0][5][19] == T_BOX_BR);
      CHECK(d_snap[0][3][0] == T_BOX_L && d_snap[0][3][19] == T_BOX_R);
      CHECK(snap_is(0, 1, "BUG") && d_snapa[0][1][1] == TXT_GOLD && d_snapa[0][2][1] == TXT_WHITE);
      CHECK(snap_is(0, 2, "ONE") && snap_is(0, 3, "TWO") && snap_is(0, 4, "THREE"));
      CHECK(snap_is(1, 1, "BUG") && snap_is(1, 2, "FOUR") && snap_is(1, 3, "") && snap_is(1, 4, ""));    /* page 2: old text cleared */
      CHECK(d_blips == 3 + 3 + 5 + 4);                                                                   /* one blip per visible char */
      CHECK(stub_win == 1 && stub_wy == HUD_Y && wmap[0][0] == T_HEART && wrow_is(0, 11, "LV 1"));      /* HUD is back and redrawn */
    }
    /* no name: 4 lines per page, starting on row 1; no HUD showing -> window hides afterwards */
    hud_hide(); dlg_set_blip(0);
    d_run(0, "A\nB\nC\nD\nE", 0);
    CHECK(d_snaps == 2 && snap_is(0, 1, "A") && snap_is(0, 4, "D") && snap_is(1, 1, "E") && snap_is(1, 2, ""));
    CHECK(stub_win == 0);
    d_run("X", "", 0); CHECK(d_snaps == 0 && stub_win == 0);                                          /* empty text opens nothing to wait on */
    /* rush: A during typing finishes the page early */
    { const char *t = "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG AGAIN";
      d_rush_at = -1; int slow = d_run("BUG", t, 3);
      d_rush_at = 40; int fast = d_run("BUG", t, 3); d_rush_at = -1;
      CHECK(slow > 100 && fast < slow - 50);
      CHECK(snap_is(0, 2, "THE QUICK BROWN") && snap_is(0, 3, "FOX JUMPS OVER THE") && snap_is(0, 4, "LAZY DOG AGAIN"));   /* rushed text is complete */
    }
    dlg_set_speed(2);
    /* ---- SYSTEM menu */
    memset(sram_buf, 0xFF, sizeof sram_buf); save_init(); new_game(0, 1, 0, 0); cur_slot = 0; memset(&sv.rsv, 0, sizeof sv.rsv); sv.friends = sv.deepest = sv.potions = 0;
    CHECK(SM_WX == 71 && SM_WY == 64);
    { /* open with the HUD showing, resume with B: layout, then HUD is back */
      static const UINT8 q1[] = { J_B, 0xFF };
      hud_init(); sv.level = 1; sv.hp = 10; hud_update();
      CHECK(sm_run(q1) == SYS_RESUME);
      CHECK(sq_wx[0] == SM_WX && stub_wx == 7 && stub_wy == HUD_Y && stub_win == 1 && wmap[0][0] == T_HEART && wrow_is(0, 11, "LV 1"));
      CHECK(sq_snap[0][0][0] == T_BOX_TL && sq_snap[0][0][11] == T_BOX_TR && sq_snap[0][9][0] == T_BOX_BL && sq_snap[0][9][11] == T_BOX_BR);
      CHECK(sq_is(0, 1, 1, "SYSTEM") && sq_is(0, 3, 2, "RESUME") && sq_is(0, 4, 2, "SAVE") && sq_is(0, 5, 2, "OPTIONS") && sq_is(0, 6, 2, "MIRROR") && sq_is(0, 7, 2, "SKILLS") && sq_is(0, 8, 2, "TO TITLE"));
      CHECK(sq_snap[0][3][1] == T_CURSOR && sq_snap[0][4][1] == ' ');
      CHECK(wamap[0][0] == TXT_WHITE);
    }
    { /* no HUD: window hides afterwards. START resumes too */
      static const UINT8 q2[] = { J_START, 0xFF };
      hud_hide(); CHECK(sm_run(q2) == SYS_RESUME); CHECK(stub_win == 0);
      static const UINT8 q2b[] = { J_A, 0xFF };                         /* A on RESUME */
      CHECK(sm_run(q2b) == SYS_RESUME);
    }
    { /* DOWN, A = save; stays open showing SAVED!; cursor moved; then B */
      static const UINT8 q3[] = { J_DOWN, J_A, J_B, 0xFF };
      UINT16 before = 0; Save t; slot_peek(0, &t); before = t.saves;
      sv.level = 17;
      CHECK(sm_run(q3) == SYS_RESUME);
      CHECK(sq_snap[2][4][1] == T_CURSOR && sq_snap[2][3][1] == ' ');
      CHECK(sq_is(4, 2, 1, "SAVED!") && sq_is(4, 4, 1, "") && sq_snap[4][4][1] == T_CURSOR);
      slot_peek(0, &t); CHECK(t.saves == before + 1 && t.level == 17);
    }
    { /* cursor wraps both ways and clears the status line */
      static const UINT8 q4[] = { J_UP, J_DOWN, J_DOWN, J_B, 0xFF };
      CHECK(sm_run(q4) == SYS_RESUME);
      CHECK(sq_snap[2][8][1] == T_CURSOR && sq_snap[4][3][1] == T_CURSOR && sq_snap[6][4][1] == T_CURSOR);
    }
    { /* OPTIONS: overlay hides, options page runs (B inside it), repaint hook fires, overlay returns */
      static const UINT8 q5[] = { J_DOWN, J_DOWN, J_A, J_B, J_B, 0xFF };
      opt.sfx = 1; hud_hide();
      CHECK(sm_run(q5) == SYS_RESUME);
      CHECK(redraws == 1 && sq_win[6] == 0 /* hidden while the options page polls */ && sq_win[8] == 1 && stub_win == 0);
    }
    { /* TO TITLE: B cancels the prompt, A confirms, autosave writes the slot */
      static const UINT8 q6[] = { J_UP, J_A, J_B, J_B, 0xFF };
      CHECK(sm_run(q6) == SYS_RESUME);
      CHECK(sq_is(4, 4, 1, "SURE?") && sq_is(4, 5, 1, " A: YES") && sq_is(4, 6, 1, " B: NO"));   /* prompt shows while waiting */
      CHECK(sq_snap[6][8][1] == T_CURSOR && sq_is(6, 8, 2, "TO TITLE") && sq_is(6, 4, 2, "SAVE"));  /* B cancelled: list restored */
      static const UINT8 q7[] = { J_UP, J_A, J_A, 0xFF };
      Save t; opt.autosave = 1; sv.level = 33; hud_hide();
      CHECK(sm_run(q7) == SYS_TITLE);
      slot_peek(0, &t); CHECK(t.level == 33);
      opt.autosave = 0; sv.level = 44; hud_hide();
      CHECK(sm_run(q7) == SYS_TITLE);
      slot_peek(0, &t); CHECK(t.level == 33);                                  /* autosave off: not written */
      CHECK(stub_win == 0);
    }
    /* ---- a version 2 save (13-part bug) is converted */
    { typedef struct { UINT32 magic; UINT16 ver; UINT32 seed; UINT8 zone; UINT16 level; UINT16 hp, mp; UINT32 gold, xp, secs; UINT16 saves; char name[9];
        UINT16 col[BC_N]; UINT8 pt[13]; UINT8 friends, deepest, potions, star, cores, story, freed[8]; UINT16 warp_best; UINT8 rsv[7]; UINT32 sum; } S2;
      S2 o; Save t; UINT32 h = 2166136261UL; UINT8 *b = (UINT8 *)&o; UINT16 i;
      memset(&o, 0, sizeof o); o.magic = 0x43475542UL; o.ver = 2; o.seed = 777; o.level = 9; o.gold = 1234; strcpy(o.name, "OLDIE"); o.col[0] = 0x1234; o.pt[1] = 3; o.pt[12] = 7; o.cores = 2; o.warp_best = 55;
      for(i = 0; i < sizeof o - 4; i++){ h ^= b[i]; h *= 16777619UL; } o.sum = h;
      memset(sram_buf, 0xFF, sizeof sram_buf); stub_bank = 1; memcpy(sram_buf + 8192, &o, sizeof o);
      CHECK(slot_peek(1, &t) && t.ver == 4 && t.seed == 777 && t.level == 9 && t.gold == 1234 && !strcmp(t.name, "OLDIE") && t.look.col[0] == 0x1234 && t.look.pt[1] == 3 && t.look.pt[12] == 7
            && t.look.pt[BP_CHEEK] == 0 && t.look.pt[BP_NECK] == 0 && t.look.pt[BP_BACK] == 0 && t.cores == 2 && t.warp_best == 55);
    }
    /* ---- a version 3 save (no upgrades) is converted, and a version 4 save keeps its upgrades */
    { typedef struct { UINT32 magic; UINT16 ver; UINT32 seed; UINT8 zone; UINT16 level; UINT16 hp, mp; UINT32 gold, xp, secs; UINT16 saves; char name[9];
        BugLook look; UINT8 friends, deepest, potions, star, cores, story, freed[8]; UINT16 warp_best; UINT8 rsv[4]; UINT32 sum; } S3;
      S3 o; Save t; UINT32 h = 2166136261UL; UINT8 *b = (UINT8 *)&o; UINT16 i;
      memset(&o, 0, sizeof o); o.magic = 0x43475542UL; o.ver = 3; o.seed = 4242; o.level = 12; o.gold = 99; strcpy(o.name, "THREE"); o.look.pt[BP_NECK] = 2; o.potions = 4;
      for(i = 0; i < sizeof o - 4; i++){ h ^= b[i]; h *= 16777619UL; } o.sum = h;
      memset(sram_buf, 0xFF, sizeof sram_buf); stub_bank = 1; memcpy(sram_buf + 8192, &o, sizeof o);
      CHECK(slot_peek(1, &t) && t.ver == 4 && t.seed == 4242 && t.level == 12 && t.gold == 99 && !strcmp(t.name, "THREE") && t.look.pt[BP_NECK] == 2 && t.potions == 4);
      { UINT8 k, z = 1; for(k = 0; k < sizeof t.up; k++) if(t.up[k]) z = 0; CHECK(z); }
      new_game(1, 5, 0, 0); cur_slot = 1; sv.up[SK_MIGHT] = 7; sv.up[UP_HULL] = 3; slot_write(1);
      CHECK(slot_peek(1, &t) && t.up[SK_MIGHT] == 7 && t.up[UP_HULL] == 3 && t.up[SK_GRIT] == 0);
      CHECK(hud_hpmax(1) == 10); sv.up[SK_GRIT] = 5; CHECK(hud_hpmax(1) == 30); sv.up[SK_GRIT] = 0;
    }
    /* ---- the bug builder: nothing may need more than the sprite budget on one scanline, whatever the parts */
    { BugLook l; UINT8 i, k, v, ok = 1;
      for(i = 0; i < BUG_NPRESET; i++){ bug_default(&l); bug_preset(&l, i, 0, 0); bug_clamp(&l); bug_build(&l); if(bug_peak() > BUG_BUDGET || !bug_set.n) ok = 0; }
      CHECK(ok);
      ok = 1;
      for(k = 0; k < BP_N; k++) for(v = 0; v < bug_part_cnt(k); v++){
          bug_default(&l); l.pt[BP_LENGTH] = 2; l.pt[BP_TAIL] = 1; l.pt[k] = v; bug_build(&l);
          if(bug_peak() > BUG_BUDGET || bug_set.n > BUG_MAXCELL) ok = 0; }
      CHECK(ok);
      bug_default(&l); l.pt[BP_BACK] = 2; bug_build(&l); { UINT8 any = 0, x, y; for(y = 0; y < 16; y++) for(x = 0; x < BUG_BW; x++) if(bug_pixel(x, y)) any = 1; CHECK(any); }   /* the back gear is up in the wing cells */
    }
    fprintf(stdout, "%s\n", fails ? "FAILED" : "ALL OK");
    return fails;
}
