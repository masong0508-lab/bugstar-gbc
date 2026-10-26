#pragma bank 30
/* WILD, part 4: TOWN EXTRAS. Four more places for the cities (town.c lists them after its own): the LIBRARY (a lore page for gold; all 8 read pays
   XP), the TAILOR (dye your shell, marks or wings), the BANK (keep gold safe: 1% every 5 minutes of play) and the ARCADE (5 quick shots at a
   moving light). Same look as the rest of the town (the window layer). The bank, the library page and the arcade best live in the WILD block. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "wram.h"
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "bug.h"
#include "bugspr.h"
#include "wild.h"

static const char *const nm[4] = { "LIBRARY", "TAILOR", "BANK", "ARCADE" };
void town2_name(UINT8 id, char *out) BANKED { scat(out, nm[id - SV2_LIBRARY]); }
UINT8 town2_open(UINT8 id, UINT8 night) BANKED { return id == SV2_ARCADE || !night; }
UINT8 town2_add(UINT8 *ids, UINT8 n, UINT8 tier, UINT16 th) BANKED {
    (void)th;
    if(tier >= 1) ids[n++] = SV2_LIBRARY;
    if(tier >= 2){ ids[n++] = SV2_TAILOR; ids[n++] = SV2_ARCADE; }
    if(tier >= 3) ids[n++] = SV2_BANK;
    return n;
}

static void line(UINT8 y, const char *s, UINT8 pal){ txt_win(1); txt_line(1, y, s, 18); txt_color(1, y, 18, 1, pal); txt_win(0); }
static void page(const char *title){
    char b[16], *d;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 18);
    txt_line(1, 1, title, 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    d = u32s(scat(b, "GOLD "), sv.gold > 999999UL ? 999999UL : sv.gold); *d = 0; txt_line(1, 2, b, 18); txt_color(1, 2, 18, 1, TXT_GOLD);
    txt_win(0); move_win(7, 0); SHOW_WIN;
}
static UINT16 rs = 1;
static UINT8 rnd(void){ rs ^= rs << 7; rs ^= rs >> 9; rs ^= rs << 8; return (UINT8)rs; }
static UINT8 a_set(UINT8 f, UINT8 v){                            /* one byte of the WILD block: 0 lib, 1 arc, 2 visits: read (f | 0x80 is a write of v) */
    UINT8 r = 0;
    wild_load();
    wram_bank(2);
    if(f == 0) r = XAP->lib; else if(f == 1) r = XAP->arc; else if(f == 2) r = XAP->visits;
    else if(f == 0x80) XAP->lib = v; else if(f == 0x81) XAP->arc = v; else XAP->visits = v;
    wram_bank(1);
    if(f & 0x80) wild_save();
    return r;
}

/* ---- the library */
static const char lore[8][3][19] = {
    { "ONE STAR IN NINE", "HAS A WEIRD TRAIT.", "WATCH THE COLOURS." },
    { "GOLDEN WORLDS PAY", "MORE FOR EVERY", "KILL. FIERCE BITE." },
    { "HEALING AIR MENDS", "YOU. BAD AIR HURTS", "BUT NEVER KILLS." },
    { "SHY ANIMALS RUN.", "FRIENDLY ONES LIKE", "A PAT: PRESS A." },
    { "CURIOUS ONES COME", "TO LOOK AT YOU. A", "PAT PAYS A BIT." },
    { "THE BANK PAYS 1%", "EVERY 5 MINUTES", "OF PLAY. SAVE UP." },
    { "THE TAILOR DYES", "YOUR SHELL. NEW", "COLOURS, SAME BUG." },
    { "THE BESTIARY IN", "EXTRAS COUNTS EVERY", "KILL ON EVERY WORLD" } };
static void library(UINT8 tier){
    UINT8 lib = a_set(0, 0), v = lib ? lib - 1 : 0, cost = 6 + tier * 2, i; char b[24], *d; const char *m = "";
    for(;;){
        page("LIBRARY");
        if(lib){ d = u16s(scat(b, "PAGE "), v + 1); d = scat(d, " OF 8"); *d = 0; line(4, b, TXT_GREY); for(i = 0; i < 3; i++) line(6 + i, lore[v][i], TXT_WHITE); }
        else line(5, "OLD BOOKS LINE THE WALLS.", TXT_WHITE);
        if(lib < 8){ d = u16s(scat(b, "A: READ MORE, "), cost); d = scat(d, "G"); *d = 0; line(11, b, TXT_GOLD); } else line(11, "YOU READ ALL OF IT.", TXT_GOLD);
        line(13, m, TXT_RED);
        line(15, "L/R: PAGES  B: BACK", TXT_GREY);
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_LEFT) && v){ v--; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_RIGHT) && lib && v + 1 < lib){ v++; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_A) && lib < 8){
                if(sv.gold < cost){ m = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
                else { sv.gold -= cost; lib++; v = lib - 1; a_set(0x80, lib); sfx(SFX_OK); m = "";
                       if(lib == 8){ gain_xp(30 + sv.level * 5); m = "ALL READ! BONUS XP"; sfx(SFX_FANFARE); } }
                break;
            }
        }
    }
}

/* ---- the tailor */
static const char *const dye_nm[8] = { "RED", "ORANGE", "GOLD", "GREEN", "TEAL", "BLUE", "PURPLE", "PINK" };
static const UINT16 dye[8] = { RGB(28,4,4), RGB(30,16,2), RGB(30,26,4), RGB(6,24,6), RGB(4,24,22), RGB(6,10,28), RGB(20,6,26), RGB(30,14,22) };
static const char *const part_nm[3] = { "SHELL", "MARKS", "WINGS" };
static const UINT8 part_c[3] = { BC_SHELL, BC_MARK, BC_WING };
static void tailor(UINT8 tier){
    UINT8 p = 0, q = 0; UINT16 cost = 20 + tier * 5; char b[24], *d; const char *m = "";
    for(;;){
        page("TAILOR");
        line(4, "DYE YOUR BUG!", TXT_WHITE);
        d = scat(b, "PART  "); d = scat(d, part_nm[p]); *d = 0; line(6, b, TXT_GOLD);
        d = scat(b, "DYE   "); d = scat(d, dye_nm[q]); *d = 0; line(8, b, TXT_GOLD);
        d = u16s(scat(b, "PRICE "), cost); d = scat(d, " GOLD"); *d = 0; line(10, b, TXT_GREY);
        line(12, m, TXT_GOLD);
        line(14, "L/R PART  U/D DYE", TXT_GREY); line(15, "A: BUY  B: BACK", TXT_GREY);
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_LEFT)){ p = p ? p - 1 : 2; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_RIGHT)){ p = p < 2 ? p + 1 : 0; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_UP)){ q = q ? q - 1 : 7; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_DOWN)){ q = q < 7 ? q + 1 : 0; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_A)){
                if(sv.gold < cost){ m = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
                else { sv.gold -= cost; sv.look.col[part_c[p]] = dye[q]; bugspr_load(&sv.look); HIDE_SPRITES; m = "A NEW LOOK! LOVELY."; sfx(SFX_FANFARE); }
                break;
            }
        }
    }
}

/* ---- the bank */
static void bank(void){
    UINT8 act = 0, ai = 1, cur = (UINT8)(play_secs() / 300), st, first; UINT16 bal; UINT32 t; char b[24], *d; const char *m = "";
    static const UINT16 amt[4] = { 10, 50, 200, 1000 };
    wild_load(); wram_bank(2); bal = XAP->bank; st = (UINT8)(cur - XAP->visits); first = !XAP->bank; wram_bank(1);
    if(first) st = 0;
    if(st > 24) st = 24;
    if(st && bal >= 50){ t = (UINT32)bal * st / 100; if(t + bal > 60000UL) t = 60000UL - bal; if(t){ bal += (UINT16)t; m = "INTEREST PAID!"; } }
    wild_load(); wram_bank(2); XAP->bank = bal; XAP->visits = cur; wram_bank(1); wild_save();
    for(;;){
        page("BANK");
        d = u16s(scat(b, "SAFE "), bal); d = scat(d, " GOLD"); *d = 0; line(4, b, TXT_WHITE);
        line(5, "1% EVERY 5 MINUTES", TXT_GREY);
        line(7, act ? " DEPOSIT" : "> DEPOSIT", act ? TXT_WHITE : TXT_GOLD); line(8, act ? "> WITHDRAW" : " WITHDRAW", act ? TXT_GOLD : TXT_WHITE);
        d = u16s(scat(b, "AMOUNT "), amt[ai]); *d = 0; line(10, b, TXT_GOLD);
        line(12, m, TXT_GOLD);
        line(14, "U/D ACT  L/R AMOUNT", TXT_GREY); line(15, "A: DO IT  B: BACK", TXT_GREY);
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_UP) || HIT(J_DOWN)){ act ^= 1; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_LEFT)){ ai = ai ? ai - 1 : 3; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_RIGHT)){ ai = ai < 3 ? ai + 1 : 0; sfx(SFX_MOVE); m = ""; break; }
            if(HIT(J_A)){
                UINT16 n = amt[ai];
                if(act == 0){
                    if(sv.gold < n) m = "NOT ENOUGH GOLD";
                    else if((UINT32)bal + n > 60000UL) m = "THE SAFE IS FULL";
                    else { sv.gold -= n; bal += n; m = "SAVED."; }
                } else {
                    if(bal < n) m = "NOT ENOUGH SAVED"; else { bal -= n; sv.gold += n; m = "HERE YOU GO."; }
                }
                sfx(m[0] == 'S' || m[0] == 'H' ? SFX_OK : SFX_BACK);
                wild_load(); wram_bank(2); XAP->bank = bal; wram_bank(1); wild_save();
                break;
            }
        }
    }
}

/* ---- the arcade */
static void arcade(void){
    UINT8 hits, r, m, dir, t, tick, zone, pos, best = a_set(1, 0), done; char b[24], bar[19], *d; const char *msg = "";
    static const UINT8 pay_[6] = { 0, 0, 10, 20, 40, 80 };
    rs ^= (UINT16)frames32() | 1;
    for(;;){
        page("ARCADE");
        line(4, "5 SHOTS AT THE LIGHT.", TXT_WHITE);
        line(5, "PRESS A IN THE ZONE.", TXT_WHITE);
        line(7, "PLAY: 8 GOLD", TXT_GOLD);
        d = u16s(scat(b, "BEST "), best); d = scat(d, " OF 5"); *d = 0; line(9, b, TXT_GREY);
        line(11, "2: 10  3: 20  4: 40", TXT_GREY); line(12, "5 HITS: 80 GOLD", TXT_GREY);
        line(14, msg, TXT_GOLD);
        line(15, "A: PLAY  B: BACK", TXT_GREY);
        for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_B) || HIT(J_START)) return; if(HIT(J_A)) break; }
        if(sv.gold < 8){ msg = "NOT ENOUGH GOLD"; sfx(SFX_BACK); continue; }
        sv.gold -= 8; hits = 0;
        for(r = 0; r < 5; r++){
            zone = 2 + rnd() % 12; m = 0; dir = 1; tick = 0; done = 0; pos = 0;
            d = u16s(scat(b, "SHOT "), r + 1); d = scat(d, " OF 5"); *d = 0; line(5, b, TXT_WHITE); line(4, "", TXT_WHITE); line(7, "", TXT_WHITE);
            while(!done){
                wait_vbl_done(); input_poll();
                if(++tick >= (UINT8)(4 - (r >> 1))){ tick = 0; if(dir){ if(++m >= 17) dir = 0; } else { if(m == 0 || --m == 0) dir = 1; } }
                for(pos = 0; pos < 18; pos++) bar[pos] = pos == m ? '*' : (pos >= zone && pos < zone + 3) ? 'O' : '.';
                bar[18] = 0; line(9, bar, TXT_WHITE); txt_win(1); txt_color(1 + zone, 9, 3, 1, TXT_GOLD); txt_win(0);
                if(HIT(J_A)){ done = 1; if(m >= zone && m < zone + 3){ hits++; sfx(SFX_OK); line(12, "HIT!", TXT_GOLD); } else { sfx(SFX_BACK); line(12, "MISS", TXT_RED); } }
            }
            for(t = 0; t < 30; t++) wait_vbl_done();
            line(12, "", TXT_WHITE);
        }
        { UINT16 g = pay_[hits];
          sv.gold += g;
          if(hits > best){ best = hits; a_set(0x81, best); }
          d = u16s(scat(b, "HITS "), hits); d = scat(d, g ? ": +" : ": "); d = u16s(d, g); *d = 0; msg = g ? "NICE SHOOTING!" : "BETTER LUCK NEXT TIME";
          line(12, b, TXT_GOLD); if(g) sfx(SFX_FANFARE); for(t = 0; t < 70; t++) wait_vbl_done(); }
    }
}

void town2_go(UINT8 id, UINT8 tier, UINT16 th) BANKED {
    (void)th;
    if(id == SV2_LIBRARY) library(tier);
    else if(id == SV2_TAILOR) tailor(tier);
    else if(id == SV2_BANK) bank();
    else arcade();
}
