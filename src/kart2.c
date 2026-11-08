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
#include "bug.h"
#include "kart.h"
#define KART_ART_NO_TABLES
#include "kart_art.h"          /* the tiles, road map and sprites (the per-frame road tables are in kart.c, bank 24) */

/* KART WORLD's menus and records (bank 25): GRAND PRIX, TIME TRIAL, QUICK RACE, the track list, FORWARD / REVERSE, 1 / 3 / 5 laps, the cup, the best times.
   The race itself is in kart.c (bank 24). */
static const char tname[6][12] = { "MEADOW", "DESERT", "SNOW PEAK", "LAVA RIM", "CANDY COAST", "NIGHT CITY" };
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

/* ---- the records: every track, each way round. Own block in the slot's SRAM bank (offset 3072), own magic and checksum, so older saves just start empty. [way * 3 + track] */
typedef struct { UINT32 magic, seed; UINT16 bl[12], rt[12], tt[12]; UINT16 races, wins; UINT8 cup2, pad; UINT32 sum; } KS2;   /* [way * 6 + track]; cup2: the best cup of the second grand prix (tracks 4..6) */
#define KS2_OFF   3072U
#define KS2_MAGIC 0x33534B42UL                                         /* "BKS2" */
static KS2 k2;
static void k2_save(void){
    const UINT8 *p = (const UINT8 *)&k2; UINT16 i;
    k2.magic = KS2_MAGIC; k2.seed = sv.seed; k2.sum = fnv(&k2, sizeof k2 - 4);
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof k2; i++) *(volatile UINT8 *)(0xA000U + KS2_OFF + i) = p[i];
    DISABLE_RAM;
}
static void k2_load(void){
    UINT8 *p = (UINT8 *)&k2; UINT16 i;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof k2; i++) p[i] = *(volatile UINT8 *)(0xA000U + KS2_OFF + i);
    DISABLE_RAM;
    if(k2.magic != KS2_MAGIC || k2.seed != sv.seed || k2.sum != fnv(&k2, sizeof k2 - 4)){      /* none yet (or an older save): start from the old forward trial times */
        memset(&k2, 0, sizeof k2); for(i = 0; i < 3; i++) k2.tt[i] = ks.tt[i];
    }
}
UINT16 kart_rec(UINT8 kind, UINT8 t, UINT8 rev) BANKED {
    UINT8 ri = (rev ? 6 : 0) + (t % 6);
    return kind == 0 ? k2.bl[ri] : kind == 1 ? k2.rt[ri] : k2.tt[ri];
}
UINT16 kart_count(UINT8 wins) BANKED { ks_load(); k2_load(); return wins ? k2.wins : k2.races; }
void kart_rec_reset(void) BANKED { memset(&k2, 0, sizeof k2); k2_save(); }

static const UINT16 prize[4] = { 200, 100, 50, 20 };
static const char *const waynm[2] = { "FORWARD", "REVERSE" };
void kart_results(UINT8 place, UINT8 gpn) BANKED {
    char b[21], *d; UINT8 ri = (kart_rev ? 6 : 0) + kart_trk, nl = kart_nlaps, nbl = 0, nrc = 0; UINT16 pz = 0;
    if(best_run && (!k2.bl[ri] || best_run < k2.bl[ri])){ k2.bl[ri] = best_run; nbl = 1; }
    if(best_run && (!EX_KART || best_run < EX_KART)) EX_KART = best_run;
    if(nl == 3){                                                    /* the total times count only over the standard three laps */
        if(rivals_on){ if(!k2.rt[ri] || tot_t < k2.rt[ri]){ k2.rt[ri] = tot_t; nrc = 1; } }
        else if(!k2.tt[ri] || tot_t < k2.tt[ri]){ k2.tt[ri] = tot_t; nrc = 1; if(!kart_rev && kart_trk < 3){ ks.tt[kart_trk] = tot_t; kart_ks_save(); } }
    }
    k2.races++; if(rivals_on && place == 1) k2.wins++;
    if(rivals_on){ pz = (UINT16)((UINT32)prize[place - 1] * nl / 3); if(kart_trk >= 3) pz += pz >> 1; if(kart_rev) pz += pz >> 1; if(!pz) pz = 5; sv.gold += pz; }
    k2_save(); if(opt.autosave) slot_write(cur_slot);
    txt_win(1); txt_soft(); txt_box(0, 0, 20, 15);
    if(gpn){ d = scat(b, "RACE "); d = u16s(d, gpn); d = scat(d, " OF 3 DONE"); *d = 0; txt_line(3, 1, b, 16); }
    else txt_line(5, 1, "RACE OVER!", 10);
    txt_color(3, 1, 14, 1, TXT_GOLD);
    if(rivals_on){ if(place == 1) d = scat(b, "YOU WON!"); else { d = scat(b, "YOU CAME "); d = u16s(d, place); d = scat(d, " OF 4"); } *d = 0; txt_line(3, 3, b, 15); if(place == 1) txt_color(3, 3, 15, 1, TXT_GOLD); }
    else txt_line(3, 3, "TIME TRIAL", 15);
    d = scat(b, "TIME "); d = ftime(d, tot_t); if(nrc) d = scat(d, " NEW"); *d = 0; txt_line(3, 5, b, 15); if(nrc) txt_color(3, 5, 15, 1, TXT_GOLD);
    d = scat(b, "LAP "); d = ftime(d, best_run); if(nbl) d = scat(d, " NEW"); *d = 0; txt_line(3, 7, b, 15); if(nbl) txt_color(3, 7, 15, 1, TXT_GOLD);
    d = u16s(b, nl); d = scat(d, nl == 1 ? " LAP, " : " LAPS, "); d = scat(d, waynm[kart_rev & 1]); *d = 0; txt_line(3, 9, b, 15); txt_color(3, 9, 15, 1, TXT_GREY);
    if(rivals_on){ d = scat(b, "PRIZE +"); d = u16s(d, pz); d = scat(d, " G"); *d = 0; txt_line(3, 11, b, 15); txt_color(3, 11, 15, 1, TXT_GOLD); }
    else if(nl != 3) txt_line(3, 11, "RECORDS: 3 LAPS", 15), txt_color(3, 11, 15, 1, TXT_GREY);
    txt_line(3, 13, "A: OK", 15); txt_color(3, 13, 15, 1, TXT_GREY);
    txt_win(0); txt_soft_end(); move_win(7, 8); SHOW_WIN;
    sfx(place == 1 ? SFX_FANFARE : SFX_COIN);
}


/* ---------------------------------------------------------------- menus */
static void menu_box(const char *title, UINT8 h){
    txt_win(1); txt_cls(); txt_box(0, 0, 20, h); txt_line(2, 1, title, 16); txt_color(2, 1, 16, 1, TXT_GOLD);
}
static void menu_show(void){ move_win(7, 16); SHOW_WIN; }                 /* (the menu keeps drawing into the WINDOW layer: txt_win(1) stays on until it leaves. It used to switch to the background here, so the cursor and the colour blink were written into the road picture: tile 10 and palette 0 in the sky, a black box) */

static const char *const cupnm[4] = { "NONE YET", "BRONZE", "SILVER", "GOLD" };
#define NTRK() (ks.cup ? 6 : 3)                                                 /* tracks open: the second three come with any cup */
static void way_line(void){ char b[17], *d = scat(b, "WAY: "); d = scat(d, waynm[kart_rev & 1]); *d = 0; txt_line(4, 7, b, 14); }
static void mode_info(UINT8 sel){
    char b[19], *d = b;
    if(sel == 0){ d = scat(d, "CUP: "); d = scat(d, cupnm[ks.cup]); }
    else if(sel == 1){ if(!ks.cup) d = scat(d, "WIN A CUP FIRST"); else { d = scat(d, "CUP: "); d = scat(d, cupnm[k2.cup2 & 3]); } }
    else if(sel == 2) d = scat(d, "BEAT YOUR GHOST");
    else if(sel == 3) d = scat(d, "ONE RACE, PAYS GOLD");
    else if(sel == 4) d = scat(d, "LEFT/RIGHT TURNS IT");
    else { d = scat(d, "WON "); d = u16s(d, k2.wins); d = scat(d, " OF "); d = u16s(d, k2.races); }
    *d = 0; txt_line(2, 9, b, 17);
}
static UINT8 pick_mode(void){                                                 /* 0 grand prix, 1 time trial, 2 quick race, 3 exit, 4 the second grand prix */
    UINT8 sel = 0, f = 0;
    DISPLAY_OFF; txt_win(1); menu_box("KART WORLD", 15);
    txt_line(4, 3, "GRAND PRIX 1", 14); txt_line(4, 4, ks.cup ? "GRAND PRIX 2" : "GRAND PRIX 2 *", 14); txt_line(4, 5, "TIME TRIAL", 14); txt_line(4, 6, "QUICK RACE", 14); way_line(); txt_line(4, 8, "EXIT", 14);
    txt_color(2, 9, 17, 1, TXT_GREY);
    txt_line(1, 11, "A GAS  DOWN BRAKE", 18); txt_line(1, 12, "B ITEM  START QUIT", 18); txt_line(1, 13, "LONG TURN = TURBO", 18);
    txt_color(1, 11, 18, 3, TXT_GREY);
    mode_info(0); txt_put(2, 3, ">");
    menu_show(); DISPLAY_ON;
    for(;;){
        wait_vbl_done(); input_poll();
        f++; UI_BLINK(2, 3 + sel, f); if(!(f & 7)){ txt_color(2, 3, 16, 6, TXT_WHITE); txt_color(2, 3 + sel, 16, 1, TXT_GOLD); }
        if(HITR(J_DOWN)){ txt_put(2, 3 + sel, " "); sel = sel < 5 ? sel + 1 : 0; txt_put(2, 3 + sel, ">"); mode_info(sel); sfx(SFX_MOVE); }
        if(HITR(J_UP)){ txt_put(2, 3 + sel, " "); sel = sel ? sel - 1 : 5; txt_put(2, 3 + sel, ">"); mode_info(sel); sfx(SFX_MOVE); }
        if(sel == 4 && (HITR(J_LEFT) || HITR(J_RIGHT) || HIT(J_A))){ kart_rev ^= 1; way_line(); sfx(SFX_OK); continue; }
        if(HIT(J_A)){
            if(sel == 1 && !ks.cup){ sfx(SFX_BACK); continue; }
            sfx(SFX_OK); HIDE_WIN; txt_win(0); return sel == 0 ? 0 : sel == 1 ? 4 : sel == 2 ? 1 : sel == 3 ? 2 : 3; }
        if(HIT(J_B) || HIT(J_START)){ HIDE_WIN; txt_win(0); return 3; }
    }
}
static void rec_line(UINT8 y, const char *nm, UINT16 t){
    char b[19], *d = scat(b, nm); while(d - b < 8) *d++ = ' ';
    if(t) d = ftime(d, t); else d = scat(d, "-:--.-");
    *d = 0; txt_line(2, y, b, 17);
}
static void track_info(UINT8 sel, UINT8 nt){                                  /* the records of the track the arrow is on */
    UINT8 y = 3 + nt;
    if(sel >= nt){ txt_line(2, y, "", 17); txt_line(2, y + 1, "", 17); txt_line(2, y + 2, "", 17); return; }
    rec_line(y, "LAP", kart_rec(0, sel, kart_rev));
    rec_line(y + 1, "RACE", kart_rec(1, sel, kart_rev));
    rec_line(y + 2, "TRIAL", kart_rec(2, sel, kart_rev));
    txt_color(2, y, 17, 3, TXT_GOLD);
}
static void laps_line(UINT8 nt){ char b[17], *d = scat(b, "LAPS: "); d = u16s(d, kart_laps); *d = 0; txt_line(4, 2 + nt, b, 14); }
static UINT8 pick_track(UINT8 tt){                                            /* 0..5, or 255 to go back */
    UINT8 sel = 0, i, nt = NTRK();
    DISPLAY_OFF; txt_win(1); menu_box("PICK A TRACK", 15); txt_line(2, 1, tt ? "TIME TRIAL: GHOST" : "PICK A TRACK", 16);
    for(i = 0; i < nt; i++) txt_line(4, 2 + i, tname[i], 14);
    laps_line(nt);
    txt_line(1, 13, kart_laps == 3 ? "A: GO   B: BACK" : "RECORDS: 3 LAPS ONLY", 18); txt_color(1, 13, 18, 1, TXT_GREY);
    txt_put(2, 2, ">"); track_info(0, nt);
    menu_show(); DISPLAY_ON;
    for(;;){
        wait_vbl_done(); input_poll();
        if(HITR(J_DOWN)){ txt_put(2, 2 + sel, " "); sel = sel < nt ? sel + 1 : 0; txt_put(2, 2 + sel, ">"); track_info(sel, nt); sfx(SFX_MOVE); }
        if(HITR(J_UP)){ txt_put(2, 2 + sel, " "); sel = sel ? sel - 1 : nt; txt_put(2, 2 + sel, ">"); track_info(sel, nt); sfx(SFX_MOVE); }
        if(sel == nt && (HITR(J_LEFT) || HITR(J_RIGHT) || HIT(J_A))){
            if(HITR(J_LEFT)) kart_laps = kart_laps == 1 ? 5 : kart_laps == 3 ? 1 : 3; else kart_laps = kart_laps == 1 ? 3 : kart_laps == 3 ? 5 : 1;
            laps_line(nt); txt_line(1, 13, kart_laps == 3 ? "A: GO   B: BACK" : "RECORDS: 3 LAPS ONLY", 18); sfx(SFX_OK); continue;
        }
        if(HIT(J_A) && sel < nt){ sfx(SFX_OK); i = sel; HIDE_WIN; txt_win(0); return i; }
        if(HIT(J_B) || HIT(J_START)){ HIDE_WIN; txt_win(0); return 255; }
    }
}

static const char *const rname[4] = { "YOU", "RED", "BLUE", "YELLOW" };
static void trophy(const UINT8 *pts, UINT8 cupn){                                         /* the cup: standings, the trophy, the prize */
    static const UINT16 cupg[4] = { 40, 120, 250, 500 };
    UINT8 ord[4], i, j, t, tr; char b[21], *d;
    for(i = 0; i < 4; i++) ord[i] = i;
    for(i = 0; i < 3; i++) for(j = 0; j < 3 - i; j++){ if(pts[ord[j]] < pts[ord[j + 1]] || (pts[ord[j]] == pts[ord[j + 1]] && ord[j + 1] == 0)){ t = ord[j]; ord[j] = ord[j + 1]; ord[j + 1] = t; } }
    for(i = 0; i < 4 && ord[i] != 0; i++) ;
    tr = i == 0 ? 3 : i == 1 ? 2 : i == 2 ? 1 : 0;                              /* gold, silver, bronze */
    txt_win(1); txt_soft(); txt_box(0, 0, 20, 16);
    txt_line(3, 1, "GRAND PRIX OVER!", 16); txt_color(3, 1, 16, 1, TXT_GOLD);
    for(i = 0; i < 4; i++){
        d = u16s(b, i + 1); d = scat(d, "  "); d = scat(d, rname[ord[i]]); while(d - b < 11) *d++ = ' '; d = u16s(d, pts[ord[i]]); d = scat(d, " PTS"); *d = 0;
        txt_line(2, 3 + i, b, 18); if(ord[i] == 0) txt_color(2, 3 + i, 18, 1, TXT_GOLD);
    }
    d = scat(b, tr == 3 ? "GOLD CUP!" : tr == 2 ? "SILVER CUP" : tr == 1 ? "BRONZE CUP" : "NO CUP THIS TIME"); *d = 0; txt_line(2, 9, b, 16); txt_color(2, 9, 16, 1, tr ? TXT_GOLD : TXT_GREY);
    d = scat(b, "PRIZE +"); d = u16s(d, cupg[tr]); d = scat(d, " G"); *d = 0; txt_line(2, 11, b, 16); txt_color(2, 11, 16, 1, TXT_GOLD);
    if(cupn ? tr > (k2.cup2 & 3) : tr > ks.cup){ if(cupn){ k2.cup2 = tr; k2_save(); } else { ks.cup = tr; kart_ks_save(); } txt_line(2, 12, "NEW BEST CUP!", 16); txt_color(2, 12, 16, 1, TXT_GOLD); }
    txt_line(2, 14, "A: OK", 16); txt_color(2, 14, 16, 1, TXT_GREY);
    sv.gold += cupg[tr]; if(opt.autosave) slot_write(cur_slot);
    txt_win(0); txt_soft_end(); move_win(7, 8); SHOW_WIN;
    sfx(tr == 3 ? SFX_FANFARE : SFX_COIN);
#ifdef KX_AUTO
    { UINT8 w; for(w = 0; w < 200; w++) wait_vbl_done(); }
#else
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B) || HIT(J_START)) break; }
#endif
    HIDE_WIN;
}
static void grand_prix(UINT8 cupn){
    UINT8 pts[4], r, i; static const UINT8 gp[4] = { 10, 6, 3, 1 };
    for(i = 0; i < 4; i++) pts[i] = 0;
    for(r = 0; r < 3; r++){
        if(!kart_race(cupn * 3 + r, 0, r + 1)) return;
        for(i = 0; i < 4; i++) pts[i] += gp[kart_rk[i] - 1];
    }
    trophy(pts, cupn);
}

void kart_gfx(void) BANKED {                                                   /* the tiles, the road map and the sprites into video memory (the LCD is off) */
    VBK_REG = 0; txt_cls();
    VBK_REG = 1; set_bkg_data(0, KT_N, kt_bg);
    set_bkg_tiles(0, 2, 32, 16, kt_attr);
    VBK_REG = 0; set_bkg_tiles(0, 2, 32, 16, kt_map);
    SPRITES_8x16;
    set_sprite_data(128, KU_N * 2, kt_spr);
    set_sprite_palette(0, 8, kt_spal);
    set_sprite_palette_entry(5, 2, sv.look.col[BC_SHELL]);
    txt_pal_reset();                                                          /* the text colours (BG palettes 0..3): a fade-out before the race leaves them black, and the HUD and the banners were invisible */
}

void kart_run(void) BANKED {
    UINT8 was = LCDC_REG & LCDCF_OBJ16, m, t;
    DISPLAY_OFF; HIDE_WIN;
    kart_setup();
    ks_load(); k2_load();
    SHOW_BKG; DISPLAY_ON;
#ifdef KX_MODE
#ifdef KX_REV
    kart_rev = 1;
#endif
#ifdef KX_LAPS
    kart_laps = KX_LAPS;
#endif
    if(KX_MODE == 2) grand_prix(0); else kart_race(KX_TRK, KX_MODE, 0);
#else
    for(;;){
        m = pick_mode();
        if(m == 3) break;
        if(m == 0 || m == 4){ grand_prix(m == 4); }
        else { t = pick_track(m == 1); if(t != 255) kart_race(t, m == 1, 0); }
        DISPLAY_OFF; kart_pal0(); SHOW_BKG; DISPLAY_ON;
    }
#endif
    HIDE_SPRITES; HIDE_WIN; if(!was) SPRITES_8x8;
    txt_win(0); VBK_REG = 0; txt_cls();
    SCX_REG = 0; SCY_REG = 0;
}

UINT8 kart_post(UINT8 t) BANKED {                                       /* the town's race post: the same setup and teardown as kart_run, one race (three laps, forward) */
    UINT8 was = LCDC_REG & LCDCF_OBJ16, r, l0 = kart_laps, w0 = kart_rev;
    DISPLAY_OFF; HIDE_WIN;
    kart_setup(); ks_load(); k2_load();
    SHOW_BKG; DISPLAY_ON;
    kart_laps = 3; kart_rev = 0;
    r = kart_race(t % 3, 0, 0);
    kart_laps = l0; kart_rev = w0;
    DISPLAY_OFF; kart_pal0(); SHOW_BKG; DISPLAY_ON;
    HIDE_SPRITES; HIDE_WIN; if(!was) SPRITES_8x8;
    txt_win(0); VBK_REG = 0; txt_cls();
    SCX_REG = 0; SCY_REG = 0;
    return r;
}

/* ---- the stunt jump (it lives here, in bank 25, because bank 24 is full) ---- */
#define AIRN 44
static const UINT16 PADM[3] = { 450, 1100, 1700 };                     /* where the jump pads are in a lap */
static const INT8   PADL[3] = { 70, -70, 0 };
#define PADMR(i) (kart_rev ? (UINT16)(2048U - PADM[i]) : PADM[i])        /* (and the pads are where they are when the road is driven from the other end) */
UINT8 air_t, air_trick;
static UINT8 pad_done;                                                  /* a bit per pad you have already taken this pass */
void kart_air_reset(void) BANKED { air_t = air_trick = pad_done = 0; }
UINT8 kart_air(UINT16 pm, INT16 lx, INT16 v, UINT8 b) BANKED {
    UINT8 i, r = 0;
    if(air_t){
        if(b && !air_trick && air_t > 6){ air_trick = 1; sfx(SFX_MOVE); }
        if(++air_t >= AIRN){ r = air_trick; air_t = 0; air_trick = 0; sfx(SFX_THUD); }
        return r;
    }
    for(i = 0; i < 3; i++){
        INT16 d = (INT16)pm - (INT16)PADMR(i), dl = lx - PADL[i];
        if(d > 14 || d < -14){ pad_done &= ~(1 << i); continue; }
        if(pad_done & (1 << i)) continue;
        if(dl > -48 && dl < 48 && v > 120){ pad_done |= 1 << i; air_t = 1; air_trick = 0; sfx(SFX_LEVEL); }
    }
    return 0;
}
INT16 kart_pad(UINT16 pm, INT16 *lat) BANKED {
    UINT8 i, nb = 0; INT16 bd = 999, dz;
    for(i = 0; i < 3; i++){ dz = (INT16)PADMR(i) - (INT16)pm; if(dz < 0) dz += 2048; if(dz < bd){ bd = dz; nb = i; } }
    *lat = PADL[nb]; return bd;
}
