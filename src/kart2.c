#pragma bank 25
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "spell.h"
#include "kart.h"

/* KART WORLD's menus (bank 25): GRAND PRIX, TIME TRIAL, QUICK RACE, the track list and the cup. The race itself is in kart.c (bank 24). */
static const char tname[3][10] = { "MEADOW", "DESERT", "SNOW PEAK" };
static char *ftime(char *d, UINT16 t){                          /* m:ss.d from 60ths of a second */
    UINT16 s = t / 60;
    d = u16s(d, s / 60); d = scat(d, ":"); if(s % 60 < 10) d = scat(d, "0");
    d = u16s(d, s % 60); d = scat(d, "."); d = u16s(d, (t % 60) / 6); return d;
}
static UINT8 ks_sum(void){ const UINT8 *p = (const UINT8 *)&ks; UINT8 i, s = 0x5A; for(i = 0; i < sizeof ks - 1; i++) s ^= (UINT8)(p[i] + i); return s; }
static void ks_load(void){
    UINT8 *p = (UINT8 *)&ks, i;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof ks; i++) p[i] = *(volatile UINT8 *)(0xA000U + KS_OFF + i);
    DISABLE_RAM;
    if(ks.seed != sv.seed || ks.chk != ks_sum()){ memset(&ks, 0, sizeof ks); ks.seed = sv.seed; }
}
static const UINT16 prize[4] = { 200, 100, 50, 20 };
void kart_results(UINT8 place, UINT8 saved, UINT8 gpn) BANKED {
    char b[21], *d;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 15);
    if(gpn){ d = scat(b, "RACE "); d = u16s(d, gpn); d = scat(d, " OF 3 DONE"); *d = 0; txt_line(3, 1, b, 14); }
    else txt_line(5, 1, "RACE OVER!", 10);
    txt_color(3, 1, 14, 1, TXT_GOLD);
    if(rivals_on){ d = scat(b, "YOU CAME "); d = u16s(d, place); d = scat(d, place == 1 ? " (WIN!)" : " OF 4"); *d = 0; txt_line(3, 3, b, 15); }
    else txt_line(3, 3, "TIME TRIAL", 15);
    d = scat(b, "TIME "); d = ftime(d, tot_t); *d = 0; txt_line(3, 5, b, 15);
    d = scat(b, "BEST LAP "); d = ftime(d, best_run); *d = 0; txt_line(3, 7, b, 15);
    if(saved) txt_line(3, 9, "NEW RECORD!", 15), txt_color(3, 9, 15, 1, TXT_GOLD);
    if(rivals_on){ d = scat(b, "PRIZE +"); d = u16s(d, prize[place - 1]); d = scat(d, " G"); *d = 0; txt_line(3, 11, b, 15); txt_color(3, 11, 15, 1, TXT_GOLD); }
    txt_line(3, 13, "A: OK", 15); txt_color(3, 13, 15, 1, TXT_GREY);
    txt_win(0); move_win(7, 8); SHOW_WIN;
    if(rivals_on) sv.gold += prize[place - 1];
    sfx(place == 1 ? SFX_FANFARE : SFX_COIN);
}


/* ---------------------------------------------------------------- menus */
static void menu_box(const char *title, UINT8 h){
    txt_win(1); txt_cls(); txt_box(0, 0, 20, h); txt_line(2, 1, title, 16); txt_color(2, 1, 16, 1, TXT_GOLD);
}
static void menu_show(void){ txt_win(0); move_win(7, 16); SHOW_WIN; }

static const char *const cupnm[4] = { "NONE YET", "BRONZE", "SILVER", "GOLD" };
static void mode_info(UINT8 sel){
    char b[21], *d = b;
    if(sel == 0){ d = scat(d, "CUP: "); d = scat(d, cupnm[ks.cup]); }
    else if(sel == 1) d = scat(d, "BEAT YOUR TIMES");
    else if(sel == 2) d = scat(d, "ONE RACE, PAYS GOLD");
    else d = scat(d, "BACK TO EXTRAS");
    *d = 0; txt_line(2, 9, b, 16);
}
static UINT8 pick_mode(void){                                                 /* 0 grand prix, 1 time trial, 2 quick race, 3 exit */
    UINT8 sel = 0, i;
    DISPLAY_OFF; txt_win(1); menu_box("KART WORLD", 15);
    txt_line(4, 3, "GRAND PRIX", 14); txt_line(4, 4, "TIME TRIAL", 14); txt_line(4, 5, "QUICK RACE", 14); txt_line(4, 6, "EXIT", 14);
    txt_color(4, 9 + 0, 14, 1, TXT_GREY);
    txt_line(1, 11, "A GAS  DOWN BRAKE", 18); txt_line(1, 12, "B ITEM  START QUIT", 18); txt_line(1, 13, "LONG TURN = TURBO", 18);
    txt_color(1, 11, 18, 3, TXT_GREY);
    mode_info(0); txt_put(2, 3, ">");
    menu_show(); DISPLAY_ON;
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_DOWN) && sel < 3){ txt_put(2, 3 + sel, " "); sel++; txt_put(2, 3 + sel, ">"); mode_info(sel); sfx(SFX_MOVE); }
        if(HIT(J_UP) && sel > 0){ txt_put(2, 3 + sel, " "); sel--; txt_put(2, 3 + sel, ">"); mode_info(sel); sfx(SFX_MOVE); }
        if(HIT(J_A)){ sfx(SFX_OK); i = sel; HIDE_WIN; return i; }
        if(HIT(J_B) || HIT(J_START)){ HIDE_WIN; return 3; }
    }
}
static UINT8 pick_track(UINT8 tt){                                            /* 0..2, or 3 to go back */
    UINT8 sel = 0, i; char b[21], *d;
    DISPLAY_OFF; txt_win(1); menu_box("PICK A TRACK", 15);
    for(i = 0; i < 3; i++){
        d = scat(b, tname[i]); *d = 0; txt_line(4, 3 + i, b, 14);
        if(tt){ d = scat(b, tname[i]); while(d - b < 10) *d++ = ' '; if(ks.tt[i]) d = ftime(d, ks.tt[i]); else d = scat(d, "-:--.-"); *d = 0; txt_line(2, 9 + i, b, 17); }
    }
    txt_line(4, 6, "BACK", 14);
    if(tt){ txt_line(2, 8, "BEST TIMES", 12); txt_color(2, 8, 12, 1, TXT_GREY); txt_color(2, 9, 17, 3, TXT_GOLD); }
    txt_put(2, 3, ">");
    menu_show(); DISPLAY_ON;
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_DOWN) && sel < 3){ txt_put(2, 3 + sel, " "); sel++; txt_put(2, 3 + sel, ">"); sfx(SFX_MOVE); }
        if(HIT(J_UP) && sel > 0){ txt_put(2, 3 + sel, " "); sel--; txt_put(2, 3 + sel, ">"); sfx(SFX_MOVE); }
        if(HIT(J_A)){ sfx(SFX_OK); i = sel; HIDE_WIN; return i; }
        if(HIT(J_B) || HIT(J_START)){ HIDE_WIN; return 3; }
    }
}

static const char *const rname[4] = { "YOU", "RED", "BLUE", "YELLOW" };
static void trophy(const UINT8 *pts){                                         /* the cup: standings, the trophy, the prize */
    static const UINT16 cupg[4] = { 40, 120, 250, 500 };
    UINT8 ord[4], i, j, t, tr; char b[21], *d;
    for(i = 0; i < 4; i++) ord[i] = i;
    for(i = 0; i < 3; i++) for(j = 0; j < 3 - i; j++){ if(pts[ord[j]] < pts[ord[j + 1]] || (pts[ord[j]] == pts[ord[j + 1]] && ord[j + 1] == 0)){ t = ord[j]; ord[j] = ord[j + 1]; ord[j + 1] = t; } }
    for(i = 0; i < 4 && ord[i] != 0; i++) ;
    tr = i == 0 ? 3 : i == 1 ? 2 : i == 2 ? 1 : 0;                              /* gold, silver, bronze */
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 16);
    txt_line(3, 1, "GRAND PRIX OVER!", 16); txt_color(3, 1, 16, 1, TXT_GOLD);
    for(i = 0; i < 4; i++){
        d = u16s(b, i + 1); d = scat(d, "  "); d = scat(d, rname[ord[i]]); while(d - b < 11) *d++ = ' '; d = u16s(d, pts[ord[i]]); d = scat(d, " PTS"); *d = 0;
        txt_line(2, 3 + i, b, 16); if(ord[i] == 0) txt_color(2, 3 + i, 16, 1, TXT_GOLD);
    }
    d = scat(b, tr == 3 ? "GOLD CUP!" : tr == 2 ? "SILVER CUP" : tr == 1 ? "BRONZE CUP" : "NO CUP THIS TIME"); *d = 0; txt_line(2, 9, b, 16); txt_color(2, 9, 16, 1, tr ? TXT_GOLD : TXT_GREY);
    d = scat(b, "PRIZE +"); d = u16s(d, cupg[tr]); d = scat(d, " G"); *d = 0; txt_line(2, 11, b, 16); txt_color(2, 11, 16, 1, TXT_GOLD);
    if(tr > ks.cup){ ks.cup = tr; kart_ks_save(); txt_line(2, 12, "NEW BEST CUP!", 16); txt_color(2, 12, 16, 1, TXT_GOLD); }
    txt_line(2, 14, "A: OK", 16); txt_color(2, 14, 16, 1, TXT_GREY);
    sv.gold += cupg[tr]; if(opt.autosave) slot_write(cur_slot);
    txt_win(0); move_win(7, 8); SHOW_WIN;
    sfx(tr == 3 ? SFX_FANFARE : SFX_COIN);
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B) || HIT(J_START)) break; }
    HIDE_WIN;
}
static void grand_prix(void){
    UINT8 pts[4], r, i; static const UINT8 gp[4] = { 10, 6, 3, 1 };
    for(i = 0; i < 4; i++) pts[i] = 0;
    for(r = 0; r < 3; r++){
        if(!kart_race(r, 0, r + 1)) return;
        for(i = 0; i < 4; i++) pts[i] += gp[kart_rk[i] - 1];
    }
    trophy(pts);
}

void kart_run(void) BANKED {
    UINT8 was = LCDC_REG & LCDCF_OBJ16, m, t;
    DISPLAY_OFF; HIDE_WIN;
    kart_setup();
    ks_load();
    SHOW_BKG; DISPLAY_ON;
#ifdef KX_MODE
    if(KX_MODE == 2) grand_prix(); else kart_race(KX_TRK, KX_MODE, 0);
#else
    for(;;){
        m = pick_mode();
        if(m == 3) break;
        if(m == 0){ grand_prix(); }
        else { t = pick_track(m == 1); if(t < 3) kart_race(t, m == 1, 0); }
        DISPLAY_OFF; kart_pal0(); SHOW_BKG; DISPLAY_ON;
    }
#endif
    HIDE_SPRITES; HIDE_WIN; if(!was) SPRITES_8x8;
    txt_win(0); VBK_REG = 0; txt_cls();
    SCX_REG = 0; SCY_REG = 0;
}
