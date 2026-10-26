#pragma bank 26
/* EXTRAS, the small extras that fit the cartridge: MINING (a timing game with a pick), the station LOUNGE (rumours, a drink), RELICS (12 trophies
   that unlock by themselves) and the DEBUG menu (unlocked with SELECT+A on its card). Saved after the kart block, SRAM offset 432 of the slot. */
#include <gb/gb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "spell.h"
#include "kart.h"
#include "xtra.h"
#include "wild.h"

typedef struct { UINT32 seed; UINT16 relics; UINT16 ore; UINT8 best, dbg, lounge, chk; } XSave;
#define XS_OFF 432U
static XSave xs;
static UINT16 xr = 0xBEEF;
static UINT8 xrnd(void){ xr ^= xr << 7; xr ^= xr >> 9; xr ^= xr << 8; return (UINT8)xr; }
static UINT8 xs_sum(void){ const UINT8 *p = (const UINT8 *)&xs; UINT8 i, s = 0x3C; for(i = 0; i < sizeof xs - 1; i++) s ^= (UINT8)(p[i] + i * 3); return s; }
static void xs_load(void){
    UINT8 *p = (UINT8 *)&xs, i;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof xs; i++) p[i] = *(volatile UINT8 *)(0xA000U + XS_OFF + i);
    DISABLE_RAM;
    if(xs.seed != sv.seed || xs.chk != xs_sum()){ memset(&xs, 0, sizeof xs); xs.seed = sv.seed; }
    xr ^= (UINT16)frames32() | 1;
}
static void xs_save(void){
    UINT8 *p = (UINT8 *)&xs, i;
    xs.seed = sv.seed; xs.chk = xs_sum();
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof xs; i++) *(volatile UINT8 *)(0xA000U + XS_OFF + i) = p[i];
    DISABLE_RAM;
    if(opt.autosave) slot_write(cur_slot);
}
static void screen(const char *title){
    txt_win(0); txt_cls(); txt_box(0, 0, 20, 18);
    txt_line(2, 1, title, 16); txt_color(2, 1, 16, 1, TXT_GOLD);
    txt_pal_reset(); SHOW_BKG; DISPLAY_ON;
}
static void gold_line(void){ char b[21], *d = scat(b, "GOLD "); d = u16s(d, sv.gold > 65000UL ? 65000U : (UINT16)sv.gold); *d = 0; txt_line(2, 16, b, 16); txt_color(2, 16, 16, 1, TXT_GREY); }
static void wait_a(void){ for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B) || HIT(J_START)) return; } }

/* ---------------------------------------------------------------- RELICS */
#define NREL 12
static const char *const rel_nm[NREL] = { "FIRST BLOOD", "CENTURION", "ANGLER", "HELL WALKER", "MOON WALKER", "HELL KING", "MOON KING", "SPEEDSTER", "CUP HOLDER", "GOLD CUP", "SPELLBOUND", "MINER" };
static const char *const rel_rq[NREL] = { "KILL A MONSTER", "100 KILLS", "CATCH 10 FISH", "HELL DEEP FLOOR 5", "MOON DEEP FLOOR 5", "HELL DEEP FLOOR 20", "MOON DEEP FLOOR 15", "KART LAP UNDER 0:40", "ANY KART CUP", "WIN A GOLD CUP", "CAST 50 SPELLS", "MINE 30 ORES" };
static UINT8 rel_ok(UINT8 i){
    switch(i){
    case 0: return EX_KILLS >= 1;   case 1: return EX_KILLS >= 100;  case 2: return EX_FISHN >= 10;
    case 3: return EX_HELL >= 5;    case 4: return EX_MOON >= 5;     case 5: return EX_HELL >= 20;
    case 6: return EX_MOON >= 15;   case 7: return EX_KART && EX_KART < 2400;
    case 8: return ks.cup >= 1;     case 9: return ks.cup >= 3;      case 10: return EX_CASTS >= 50;
    default: return xs.ore >= 30;
    }
}
UINT8 xtra_relics(void) BANKED {                                 /* unlock what is earned (60 gold each) and count them */
    UINT8 i, n = 0, ch = 0;
    xs_load();
    for(i = 0; i < NREL; i++){
        if(!(xs.relics & (1U << i)) && rel_ok(i)){ xs.relics |= 1U << i; sv.gold += 60; ch = 1; }
        if(xs.relics & (1U << i)) n++;
    }
    if(ch) xs_save();
    return n;
}
void xtra_relic_screen(void) BANKED {
    UINT8 i, top = 0, n; char b[21], *d;
    n = xtra_relics();
    for(;;){
        screen("RELICS");
        d = u16s(b, n); d = scat(d, " OF 12"); *d = 0; txt_line(12, 1, b, 7); txt_color(12, 1, 7, 1, TXT_WHITE);
        for(i = 0; i < 5; i++){
            UINT8 k = top + i, y = 3 + i * 2, got = (xs.relics >> k) & 1;
            txt_line(2, y, got ? "*" : "-", 1); txt_color(2, y, 1, 1, got ? TXT_GOLD : TXT_GREY);
            txt_line(4, y, rel_nm[k], 14); txt_color(4, y, 14, 1, got ? TXT_GOLD : TXT_GREY);
            txt_line(4, y + 1, rel_rq[k], 14); txt_color(4, y + 1, 14, 1, TXT_GREY);
        }
        txt_line(18, 3, top ? "^" : " ", 1); txt_line(18, 12, top + 5 < NREL ? "v" : " ", 1);
        txt_line(2, 15, "UP/DOWN  B: BACK", 16); txt_color(2, 15, 16, 1, TXT_GREY);
        for(;;){ wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START) || HIT(J_A)) return;
            if(HIT(J_DOWN) && top + 5 < NREL){ top += 5; if(top + 5 > NREL) top = NREL - 5; sfx(SFX_MOVE); break; }
            if(HIT(J_UP) && top){ top = top > 5 ? top - 5 : 0; sfx(SFX_MOVE); break; } }
    }
}

/* ---------------------------------------------------------------- MINING: press A when the pick is over the ore */
void xtra_mine(void) BANKED {
    UINT8 sw, m, dir, t, tick, hit, dist, got = 0, ok; UINT16 pay = 0, ores = 0; char b[21], bar[21], *d;
    static const char *const nm[5] = { "A ROCK. NOTHING.", "COPPER! +4", "SILVER! +10", "GOLD! +20", "A GEM! +40" };
    static const UINT8 val[5] = { 0, 4, 10, 20, 40 };
    xs_load();
    screen("MINING");
    txt_line(2, 3, "PRESS A WHEN THE PICK", 18); txt_line(2, 4, "IS OVER THE ORE: [ ]", 18); txt_color(2, 3, 18, 2, TXT_GREY);
    txt_line(2, 14, "8 SWINGS. A: START", 18); txt_color(2, 14, 18, 1, TXT_GREY);
    wait_a();
    for(sw = 0; sw < 8; sw++){
        t = 3 + xrnd() % 10; m = 0; dir = 1; tick = 0; hit = 0;
        d = scat(b, "SWING "); d = u16s(d, sw + 1); d = scat(d, " OF 8"); *d = 0; txt_line(2, 3, b, 18); txt_color(2, 3, 18, 1, TXT_WHITE); txt_line(2, 4, "", 18);
        txt_line(2, 14, "", 18);
        while(!hit){
            wait_vbl_done(); input_poll();
            if(++tick >= (UINT8)(5 - (sw >> 1))){ tick = 0; if(dir){ if(++m >= 17) dir = 0; } else { if(m == 0 || --m == 0) dir = 1; } }
            for(ok = 0; ok < 18; ok++) bar[ok] = (ok == m) ? 'V' : (ok >= t && ok <= t + 2) ? 'O' : '.';
            bar[18] = 0; txt_line(2, 8, bar, 18); txt_color(2, 8, 18, 1, TXT_WHITE); txt_color(2 + t, 8, 3, 1, TXT_GOLD);
            if(HIT(J_A)) hit = 1;
            if(HIT(J_START)){ sw = 9; hit = 2; }
        }
        if(hit == 2) break;
        dist = m > t + 1 ? m - (t + 1) : (t + 1) - m;
        ok = dist == 0 ? 4 : dist == 1 ? 3 : dist == 2 ? 2 : dist <= 4 ? 1 : 0;
        txt_line(2, 10, nm[ok], 18); txt_color(2, 10, 18, 1, ok ? TXT_GOLD : TXT_RED);
        pay += val[ok]; if(ok){ ores++; got++; } sfx(ok ? SFX_COIN : SFX_HURT);
        for(tick = 0; tick < 40; tick++) wait_vbl_done();
    }
    if(sw < 9){
        screen("MINING"); d = scat(b, "ORES "); d = u16s(d, ores); *d = 0; txt_line(4, 5, b, 14);
        pay += pay * weird_pct(weird_of(sv.star), WP_ORE) / 100;
        d = scat(b, "PAY +"); d = u16s(d, pay); d = scat(d, " G"); *d = 0; txt_line(4, 7, b, 14); txt_color(4, 7, 14, 1, TXT_GOLD);
        sv.gold += pay; xs.ore += got; if(pay > xs.best) xs.best = pay > 255 ? 255 : (UINT8)pay;
        d = scat(b, "BEST "); d = u16s(d, xs.best); *d = 0; txt_line(4, 9, b, 14); txt_color(4, 9, 14, 1, TXT_GREY);
        sfx(pay >= 100 ? SFX_FANFARE : SFX_OK); xs_save(); gold_line();
        txt_line(2, 14, "A: OK", 8); wait_a();
    }
}

/* ---------------------------------------------------------------- the station LOUNGE */
static const char *const tips[12] = {
    "SELECT CASTS YOUR\nSPELL. A+SELECT\nSWAPS SPELLS.", "B+SELECT DRINKS A\nPOTION WHEN YOU\nARE HURT.",
    "GRAB A BOOK: NEW\nSPELLS COME AT\nLEVELS 1 3 5 8.", "THE QUEEN CAN NOT\nBE KILLED BY A\nSPELL. BITE HER!",
    "SOME FLOORS ARE\nMAZES. FIND THE\nSTAIRS BY TORCH.", "FEATS PAY GOLD:\nCHECK THE FEATS\nPAGE IN SYSTEM.",
    "HELL DEEP GETS\nHARDER EVERY 4\nFLOORS. CASH OUT.", "MOON DEEP IS COOLER\nBUT PAYS MORE.\nTIER EVERY 5.",
    "KART: DRIFT LONG\nBENDS FOR A TURBO\nBOOST.", "KART: HIT A JUMP\nPAD AND PRESS B\nFOR A TRICK.",
    "FISH IN THE PONDS\nBRINGS FEATS AND\nGOLD.", "RELICS UNLOCK BY\nTHEMSELVES. EACH\nPAYS 60 GOLD." };
static void tip_show(UINT8 i){
    const char *s = tips[i % 12]; UINT8 y = 8; char b[21]; UINT8 k;
    for(;;){ for(k = 0; k < 20 && s[k] && s[k] != '\n'; k++) b[k] = s[k]; b[k] = 0; txt_line(2, y, b, 18); txt_color(2, y, 18, 1, TXT_WHITE); y++; s += k; if(!*s) break; s++; }
}
void xtra_lounge(void) BANKED {
    UINT8 sel = 0, d = 1, tip = 0, row;
    xs_load(); tip = xs.lounge;
    for(;;){
        if(d){
            screen("STATION LOUNGE");
            txt_line(2, 3, "THE BARTENDER NODS.", 18); txt_color(2, 3, 18, 1, TXT_GREY);
            for(row = 0; row < 3; row++){ txt_line(2, 5 + row, row == sel ? S_CURSOR : " ", 1); txt_line(4, 5 + row, row == 0 ? "LISTEN TO A RUMOUR" : row == 1 ? "BUY A DRINK 10G" : "LEAVE", 16); txt_color(2, 5 + row, 18, 1, row == sel ? TXT_GOLD : TXT_WHITE); }
            gold_line(); d = 0;
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) break;
        if(HIT(J_UP)){ sel = sel ? sel - 1 : 2; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_DOWN)){ sel = sel < 2 ? sel + 1 : 0; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_A)){
            if(sel == 2) break;
            if(sel == 0){ d = 1; screen("A RUMOUR"); tip_show(tip); tip = (tip + 1) % 12; xs.lounge = tip; sfx(SFX_OK); txt_line(2, 14, "A: OK", 8); wait_a(); }
            else if(sv.gold < 10){ txt_line(2, 10, "NOT ENOUGH GOLD.", 18); txt_color(2, 10, 18, 1, TXT_RED); sfx(SFX_HURT); }
            else if(sv.hp >= hud_hpmax(sv.level)){ txt_line(2, 10, "YOU ARE FINE.", 18); txt_color(2, 10, 18, 1, TXT_GREY); }
            else { sv.gold -= 10; sv.hp = hud_hpmax(sv.level); gold_line(); txt_line(2, 10, "AHH. FULL HEARTS!", 18); txt_color(2, 10, 18, 1, TXT_GOLD); sfx(SFX_LEVEL); }
        }
    }
    xs_save();
}

/* ---------------------------------------------------------------- DEBUG */
UINT8 xtra_dbg_open(UINT8 unlock) BANKED {
    xs_load();
    if(unlock && !xs.dbg){ xs.dbg = 1; xs_save(); }
    return xs.dbg;
}
void xtra_debug(void) BANKED {
    UINT8 sel = 0, d = 1, row; char b[21], *e;
    static const char *const it[6] = { "+1000 GOLD", "LEVEL UP", "FULL HEARTS", "UNLOCK ALL RELICS", "CLEAR KART TIMES", "LEAVE" };
    xs_load();
    for(;;){
        if(d){
            screen("DEBUG");
            for(row = 0; row < 6; row++){ txt_line(2, 3 + row, row == sel ? S_CURSOR : " ", 1); txt_line(4, 3 + row, it[row], 16); txt_color(2, 3 + row, 18, 1, row == sel ? TXT_GOLD : TXT_WHITE); }
            e = scat(b, "LEVEL "); e = u16s(e, sv.level); *e = 0; txt_line(2, 12, b, 16); txt_color(2, 12, 16, 1, TXT_GREY);
            gold_line(); d = 0;
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) break;
        if(HIT(J_UP)){ sel = sel ? sel - 1 : 5; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_DOWN)){ sel = sel < 5 ? sel + 1 : 0; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_A)){
            d = 1; sfx(SFX_OK);
            if(sel == 0) sv.gold += 1000;
            else if(sel == 1){ if(sv.level < 99) sv.level++; sv.hp = hud_hpmax(sv.level); }
            else if(sel == 2) sv.hp = hud_hpmax(sv.level);
            else if(sel == 3) xs.relics = 0x0FFF;
            else if(sel == 4){ memset(ks.tt, 0, sizeof ks.tt); ks.cup = 0; EX_KART = 0; kart_ks_save(); }
            else break;
        }
    }
    xs_save();
}
