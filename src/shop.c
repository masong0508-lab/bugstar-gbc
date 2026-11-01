#pragma bank 15
/* BUGSTAR GBC - upgrades. The ship's menu (tabs GO / SHOP / CARE / YOU, like the GBA), Bolt's shop and the SKILLS page.
   SKILLS: every level gives 1 skill point (and every 5th level 2 more); five skills, 10 ranks each, 1 point a rank.
   SHOP: gold buys potions and 7 pieces of gear (5 levels each): FANG and SHELL for the bug, LASER GUNS ENGINE TURBO HULL MAGNET for the ship.
   The levels live in sv.up[] (save.h). Everything is drawn in the window layer, like the SYSTEM menu. */
#include <gb/gb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "menus.h"
#include "shop.h"
#include "bolt.h"
#include "econ.h"
#include "loot.h"

#define POTION_COST 40

/* ---------------------------------------------------------------- text helpers */
static char *pad(char *d, char *start, UINT8 n){ while((UINT8)(d - start) < n) *d++ = ' '; *d = 0; return d; }
static char *pips(char *d, UINT8 have, UINT8 of){ UINT8 i; for(i = 0; i < of; i++) *d++ = i < have ? '*' : '.'; *d = 0; return d; }
static char *num(char *d, const char *pre, UINT16 v, const char *post){ d = scat(d, pre); d = u16s(d, v); return scat(d, post); }

/* ---------------------------------------------------------------- skills */
static const char *const sk_name[NPERK] = { "MIGHT", "GRIT", "GUARD", "LUCK", "VIGOR" };
static const char *const sk_what[NPERK] = { "STRONGER BITE", "MORE HEARTS", "LESS DAMAGE TAKEN", "MORE GOLD FOUND", "BETTER POTIONS" };
static void sk_effect(UINT8 i, UINT8 r, char *out){
    switch(i){
    case 0: num(out, "BITE +", r, ""); break;
    case 1: num(out, "MAX HP +", r * 4, ""); break;
    case 2: num(out, "HITS -", r / 3, r % 3 ? " (NEXT AT 3)" : ""); break;
    case 3: num(out, "GOLD +", r * 6, "%"); break;
    default: num(out, "POTION +", r * 5, "%"); break;
    }
}
UINT8 skills_free(void) BANKED {                                /* skill points not spent yet */
    UINT16 total = sv.level - 1 + (sv.level / 5) * 2, spent = 0; UINT8 i;
    for(i = 0; i < NPERK; i++) spent += sv.up[i];
    return total > spent ? (UINT8)(total - spent > 255 ? 255 : total - spent) : 0;
}
static void sk_draw(UINT8 sel, const char *msg){
    UINT8 i; char b[24], *d;
    txt_win(1);
    txt_line(1, 1, "SKILLS", 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    d = num(b, "LV ", sv.level, "  POINTS "); d = u16s(d, skills_free()); pad(d, b, 18); txt_line(1, 2, b, 18); txt_color(1, 2, 18, 1, TXT_GREY);
    for(i = 0; i < NPERK; i++){
        d = scat(b, i == sel ? S_CURSOR : " "); d = scat(d, sk_name[i]); d = pad(d, b, 8); pips(d, sv.up[i], 10);
        txt_line(1, 4 + i, b, 18); txt_color(1, 4 + i, 18, 1, i == sel ? TXT_GOLD : TXT_WHITE);
    }
    txt_line(1, 10, sk_what[sel], 18); txt_color(1, 10, 18, 1, TXT_WHITE);
    d = scat(b, "NOW  "); sk_effect(sel, sv.up[sel], d); txt_line(1, 11, b, 18); txt_color(1, 11, 18, 1, TXT_GREY);
    if(sv.up[sel] < 10){ d = scat(b, "NEXT "); sk_effect(sel, sv.up[sel] + 1, d); } else scat(b, "FULLY TRAINED");
    txt_line(1, 12, b, 18); txt_color(1, 12, 18, 1, TXT_GOLD);
    txt_line(1, 14, msg ? msg : "", 18); txt_color(1, 14, 18, 1, TXT_RED);
    txt_line(1, 15, "A:TRAIN SEL:RESET", 18); txt_color(1, 15, 18, 1, TXT_GREY); txt_line(1, 16, "B:BACK", 18); txt_color(1, 16, 18, 1, TXT_GREY);
    txt_win(0);
}
void skills_run(void) BANKED {
    UINT8 sel = 0, ask = 0, sprites = LCDC_REG & LCDCF_OBJON; char m[20];
    const char *msg = 0;
    HIDE_SPRITES;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 18); txt_win(0);
    sk_draw(sel, 0); move_win(7, 0); SHOW_WIN;
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : NPERK - 1; msg = 0; ask = 0; sfx(SFX_MOVE); sk_draw(sel, msg); }
        if(HIT(J_DOWN)) { sel = sel < NPERK - 1 ? sel + 1 : 0; msg = 0; ask = 0; sfx(SFX_MOVE); sk_draw(sel, msg); }
        if(HIT(J_B)){ if(ask){ ask = 0; msg = 0; sk_draw(sel, msg); sfx(SFX_BACK); } else break; }
        if(HIT(J_SELECT)){
            UINT8 spent = 0, i; for(i = 0; i < NPERK; i++) spent += sv.up[i];
            if(!spent){ msg = "NOTHING TO RESET"; sfx(SFX_BACK); }
            else { ask = 1; num(m, "RESET FOR ", (UINT16)sv.level * 20, "? A:YES"); msg = m; sfx(SFX_MOVE); }
            sk_draw(sel, msg);
        }
        if(HIT(J_A)){
            if(ask){
                if(sv.gold >= (UINT32)sv.level * 20){ UINT8 i; sv.gold -= (UINT32)sv.level * 20; for(i = 0; i < NPERK; i++) sv.up[i] = 0; msg = "POINTS GIVEN BACK"; sfx(SFX_COIN); }
                else { msg = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
                ask = 0;
            } else if(sv.up[sel] >= 10){ msg = "ALREADY MAXED OUT"; sfx(SFX_BACK); }
            else if(!skills_free()){ msg = "NO POINTS. LEVEL UP!"; sfx(SFX_BACK); }
            else { sv.up[sel]++; msg = 0; sfx(SFX_LEVEL);
                   if(sel == 1){ UINT16 mx = hud_hpmax(sv.level); sv.hp = sv.hp + 4 > mx ? mx : sv.hp + 4; } }
            sk_draw(sel, msg);
        }
    }
    sfx(SFX_BACK);
    txt_win(1); txt_cls(); txt_win(0);
    if(sprites) SHOW_SPRITES;                                      /* (the caller puts its own window back) */
}

/* ---------------------------------------------------------------- shop */
enum { I_POTION, I_FANG, I_SHELL, I_LASER, I_GUNS, I_ENGINE, I_TURBO, I_HULL, I_MAGNET, I_SHIELD, I_HOLD, I_TANK, I_N };
static const char *const it_name[I_N] = { "POTION", "FANG", "SHELL", "LASER", "GUNS", "ENGINE", "TURBO", "HULL", "MAGNET", "SHIELD", "HOLD", "TANK" };
static const UINT8  it_up[I_N] = { 0, UP_FANG, UP_SHELL, UP_LASER, UP_GUNS, UP_ENGINE, UP_TURBO, UP_HULL, UP_MAGNET, UP_SHIELD, UP_HOLD, UP_TANK };
static const UINT16 it_cost[I_N][5] = {
    { POTION_COST, 0, 0, 0, 0 },
    { 150, 300, 600, 1200, 2400 },     /* FANG */
    { 200, 400, 800, 1600, 3200 },     /* SHELL */
    { 120, 300, 700, 1500, 3000 },     /* LASER */
    { 250, 500, 1000, 2000, 4000 },    /* GUNS */
    { 150, 350, 700, 1400, 2800 },     /* ENGINE */
    { 150, 350, 700, 1400, 2800 },     /* TURBO */
    { 200, 450, 900, 1800, 3600 },     /* HULL */
    { 200, 400, 800, 1600, 3200 },     /* MAGNET */
    { 250, 500, 1000, 2000, 4000 },    /* SHIELD */
    { 200, 400, 800, 1600, 3200 },     /* HOLD */
    { 150, 300, 600, 1200, 2400 },     /* TANK */
};
static const char *const gun_name[6] = { "ONE SHOT", "RAPID FIRE", "TWIN SHOTS", "TWIN RAPID", "TRIPLE SHOT", "TRIPLE RAPID" };
static const char *const it_what[I_N] = { "HEALS HALF YOUR HP", "BITES HIT HARDER", "TOUGHER SHELL", "SHIP LASER POWER", "MORE LASERS", "FASTER CRUISING", "FASTER BOOSTING", "TOUGHER SHIP", "MORE GOLD IN SPACE", "ABSORBS DAMAGE", "BIGGER CARGO BAY", "BIGGER FUEL TANK" };
static void it_effect(UINT8 it, UINT8 lv, char *out){
    switch(it){
    case I_FANG:   num(out, "BITE +", lv * 2, ""); break;
    case I_SHELL:  num(out, "ARMOR -", lv, ""); break;
    case I_LASER:  num(out, "POWER ", 1 + (lv + 1) / 2, ""); break;
    case I_GUNS:   scat(out, gun_name[lv > 5 ? 5 : lv]); break;
    case I_ENGINE: num(out, "SPEED +", lv * 9, "%"); break;
    case I_TURBO:  num(out, "TURBO +", lv * 11, "%"); break;
    case I_HULL:   num(out, "ARMOR -", lv, ""); break;
    case I_MAGNET: num(out, "GOLD +", lv * 12, "%"); break;
    case I_SHIELD: num(out, "SHIELD ", lv * 8, " MP"); break;
    case I_HOLD:   num(out, "HOLD ", hold_tab[lv > 5 ? 5 : lv], " UNITS"); break;
    default:       num(out, "TANK ", tank_tab[lv > 5 ? 5 : lv], " FUEL"); break;
    }
}
#define SHOP_MAXLV 5
static UINT8 s_mode, s_n, s_first, s_disc, s_ask;                   /* 0 the planet's shop (potions, bug gear), 1 the shipyard of a station (ship parts) */
static UINT16 s_price(UINT8 it, UINT8 lv){ return (UINT16)((UINT32)it_cost[it][it ? lv : 0] * (100 - s_disc) / 100); }
static const char s_thanks[] = "THANK YOU!";
static void shop_draw(UINT8 cur, const char *msg){
    UINT8 i, it = s_first + cur, lv, r = 4 + s_n; char b[24], *d;
    txt_win(1);
    d = scat(b, s_mode ? "YARD  GOLD " : "SHOP  GOLD "); d = u32s(d, sv.gold > 999999999UL ? 999999999UL : sv.gold); pad(d, b, 18); txt_line(1, 1, b, 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    txt_line(1, 2, s_ask ? "A: YES   B: NO" : "A:BUY S:SELL >GEAR", 18); txt_color(1, 2, 18, 1, s_ask ? TXT_GOLD : TXT_GREY);   /* what the buttons do, like the GBA's footer */
    for(i = 0; i < s_n; i++){
        it = s_first + i; lv = it ? sv.up[it_up[it]] : sv.potions;
        d = scat(b, i == cur ? S_CURSOR : " "); d = scat(d, it_name[it]); d = pad(d, b, 8);
        if(it == I_POTION){ d = scat(d, "x"); d = u16s(d, lv); d = pad(d, b, 13); if(lv >= 9) scat(d, "FULL"); else num(d, "", POTION_COST, ""); }
        else { d = pips(d, lv, SHOP_MAXLV); if(lv < SHOP_MAXLV) num(d, "", s_price(it, lv), ""); else scat(d, "MAX"); }
        txt_line(1, 3 + i, b, 18); txt_color(1, 3 + i, 18, 1, i == cur ? TXT_GOLD : TXT_WHITE);
        if(it ? lv < SHOP_MAXLV : lv < 9){ if(sv.gold < s_price(it, it ? lv : 0)) txt_color(14, 3 + i, 5, 1, TXT_RED); }   /* a price you cannot pay is red */
    }
    it = s_first + cur;
    txt_line(1, r, it_what[it], 18); txt_color(1, r, 18, 1, TXT_WHITE);
    if(it == I_POTION){ txt_line(1, r + 1, "SELECT DRINKS ONE", 18); txt_color(1, r + 1, 18, 1, TXT_GREY); txt_line(1, r + 2, "", 18); }
    else {
        lv = sv.up[it_up[it]];
        d = scat(b, "NOW  "); it_effect(it, lv, d); txt_line(1, r + 1, b, 18); txt_color(1, r + 1, 18, 1, TXT_GREY);
        if(lv < SHOP_MAXLV){ d = scat(b, "NEXT "); it_effect(it, lv + 1, d); } else scat(b, "FULLY UPGRADED");
        txt_line(1, r + 2, b, 18); txt_color(1, r + 2, 18, 1, TXT_GOLD);
    }
    lv = it ? sv.up[it_up[it]] : sv.potions;
    if(s_ask){ msg = "MOST OF YOUR GOLD!"; }
    if(msg){ txt_line(1, r + 3, msg, 18); txt_color(1, r + 3, 18, 1, msg == s_thanks ? TXT_GOLD : TXT_RED); }
    else if(it ? lv < SHOP_MAXLV : lv < 9){                                   /* what the gold will be, or what is missing (the GBA's PRICE / LEFT / NEED line) */
        UINT16 pr = s_price(it, it ? lv : 0);
        if(sv.gold >= pr){ d = scat(b, "LEFT "); u32s(d, sv.gold - pr); txt_line(1, r + 3, b, 18); txt_color(1, r + 3, 18, 1, TXT_GREY); }
        else { d = scat(b, "NEED "); d = u32s(d, pr - sv.gold); scat(d, " MORE"); txt_line(1, r + 3, b, 18); txt_color(1, r + 3, 18, 1, TXT_RED); }
    } else { txt_line(1, r + 3, "", 18); }
    txt_win(0);
}
static UINT8 shop_sure(UINT8 cur){                              /* a big buy asks first (the GBA: "THAT IS MOST OF YOUR GOLD"): A yes, B no */
    UINT8 yes = 0;
    s_ask = 1; shop_draw(cur, 0); sfx(SFX_MOVE);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_A)){ yes = 1; break; }
        if(HIT(J_B) || HIT(J_START)) break;
    }
    s_ask = 0; return yes;
}
void shop_run(UINT8 mode) BANKED {                              /* mode 1 = a station's shipyard */
    UINT8 cur = 0, sprites = LCDC_REG & LCDCF_OBJON, h; const char *msg = 0;
    s_mode = mode; s_n = mode ? 9 : 3; s_first = mode ? I_LASER : I_POTION; s_disc = mode ? fame_disc() : 0; h = 9 + s_n;
    HIDE_SPRITES;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, h); txt_win(0);
    shop_draw(cur, msg); move_win(7, 144 - h * 8); SHOW_WIN;
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { cur = cur ? cur - 1 : s_n - 1; msg = 0; sfx(SFX_MOVE); shop_draw(cur, msg); }
        if(HIT(J_DOWN)) { cur = cur < s_n - 1 ? cur + 1 : 0; msg = 0; sfx(SFX_MOVE); shop_draw(cur, msg); }
        if(HIT(J_B) || HIT(J_START)) break;
        if(HIT(J_SELECT)){ loot_run(mode ? 2 : 1); txt_win(1); txt_cls(); txt_box(0, 0, 20, h); txt_win(0); msg = 0; shop_draw(cur, msg); move_win(7, 144 - h * 8); SHOW_WIN; }
        if(HIT(J_RIGHT)){ loot_run(3); txt_win(1); txt_cls(); txt_box(0, 0, 20, h); txt_win(0); msg = 0; shop_draw(cur, msg); move_win(7, 144 - h * 8); SHOW_WIN; }
        if(HIT(J_A)){
            UINT8 it = s_first + cur, lv = it ? sv.up[it_up[it]] : sv.potions, mx = it ? SHOP_MAXLV : 9;
            if(lv >= mx){ msg = it ? "ALREADY THE BEST!" : "POCKETS FULL!"; sfx(SFX_BACK); }
            else if(sv.gold < s_price(it, lv)){ msg = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
            else if(s_price(it, lv) >= 60 && (UINT32)s_price(it, lv) * 2 > sv.gold && !shop_sure(cur)){ msg = "MAYBE LATER"; sfx(SFX_BACK); }
            else {
                sv.gold -= s_price(it, lv); msg = s_thanks; sfx(SFX_COIN);
                if(!it) sv.potions++; else sv.up[it_up[it]]++;
                if(it == I_SHIELD) sv.mp = hud_mpmax(sv.level);
            }
            shop_draw(cur, msg);
        }
    }
    sfx(SFX_BACK);
    if(sprites) SHOW_SPRITES;
}

/* ---------------------------------------------------------------- the STATUS page (the GBA's BUG tab: you, your bars, your numbers) */
static void st_bar(char *b, const char *lab, UINT32 v, UINT32 mx){
    char *d = scat(b, lab); UINT8 f = mx ? (UINT8)(v >= mx ? 6 : (v * 6) / mx) : 0;
    if(v && !f) f = 1;                                              /* some hearts left always shows a pip */
    d = pips(d, f, 6); d = scat(d, " "); d = u32s(d, v); d = scat(d, "/"); u32s(d, mx);
}
void status_run(void) BANKED {
    char b[24], *d; UINT32 t = play_secs(); UINT8 r = fame_rank();
    txt_win(0); txt_cls(); txt_box(0, 0, 20, 18);
    txt_line(2, 1, "STATUS", 8); txt_color(2, 1, 8, 1, TXT_GOLD); txt_line(11, 1, "B: BACK", 8); txt_color(11, 1, 8, 1, TXT_GREY);
    d = scat(b, sv.name); d = pad(d, b, 11); num(d, "LV ", sv.level, ""); txt_line(2, 2, b, 17); txt_color(2, 2, 17, 1, TXT_WHITE); txt_color(2, 2, 8, 1, TXT_GOLD);
    st_bar(b, "HP ", sv.hp, hud_hpmax(sv.level));  txt_line(2, 4, b, 17); txt_color(2, 4, 17, 1, TXT_RED);
    st_bar(b, "MP ", sv.mp, hud_mpmax(sv.level));  txt_line(2, 5, b, 17); txt_color(2, 5, 17, 1, TXT_WHITE);
    st_bar(b, "XP ", sv.xp, (UINT32)sv.level * 12); txt_line(2, 6, b, 17); txt_color(2, 6, 17, 1, TXT_GOLD);
    d = scat(b, "GOLD  "); u32s(d, sv.gold > 999999999UL ? 999999999UL : sv.gold); txt_line(2, 8, b, 17); txt_color(2, 8, 17, 1, TXT_GOLD);
    b[0] = 0; rank_label(r, b); txt_line(2, 9, b, 17); txt_color(2, 9, 17, 1, TXT_GOLD);
    d = num(b, "FAME ", sv.el.fame, ""); d = pad(d, b, 10); num(d, "KILLS ", sv.el.kills, ""); txt_line(2, 10, b, 17); txt_color(2, 10, 17, 1, TXT_WHITE);
    d = num(b, "JOBS ", sv.el.jobs, ""); d = pad(d, b, 10); num(d, "JUMPS ", sv.el.jumps, ""); txt_line(2, 11, b, 17); txt_color(2, 11, 17, 1, TXT_WHITE);
    d = num(b, "DEEP ", sv.deepest, ""); d = pad(d, b, 8); num(d, "CORES ", sv.cores, "/8"); txt_line(2, 12, b, 17); txt_color(2, 12, 17, 1, TXT_WHITE);
    b[0] = 0; it_effect(I_FANG, sv.up[it_up[I_FANG]], b); d = b; while(*d) d++; d = pad(d, b, 9); it_effect(I_SHELL, sv.up[it_up[I_SHELL]], d); txt_line(2, 13, b, 17); txt_color(2, 13, 17, 1, TXT_WHITE);
    d = num(b, "POTIONS ", sv.potions, ""); d = pad(d, b, 11); num(d, "BUGS ", sv.friends, ""); txt_line(2, 14, b, 17); txt_color(2, 14, 17, 1, TXT_WHITE);
    num(b, "SKILL POINTS ", skills_free(), ""); txt_line(2, 15, b, 17); txt_color(2, 15, 17, 1, skills_free() ? TXT_GOLD : TXT_GREY);
    d = num(b, "TIME ", (UINT16)(t / 3600), ":"); if((t / 60) % 60 < 10) d = scat(d, "0"); u16s(d, (UINT16)((t / 60) % 60)); txt_line(2, 16, b, 17); txt_color(2, 16, 17, 1, TXT_GREY);
    txt_pal_reset(); SHOW_BKG; DISPLAY_ON;
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_B) || HIT(J_A) || HIT(J_START)) break; }
    sfx(SFX_BACK);
}

/* ---------------------------------------------------------------- the ship's menu */
/* ---------------------------------------------------------------- the ship menu (four tabs like the GBA: GO / SHOP / CARE / YOU) */
#define SM_H 7                                               /* box height in tiles: border, tabs, 3 rows, tip, border */
#define TAB_GO 0
#define TAB_SHOP 1
#define TAB_CARE 2
#define TAB_YOU 3
static const UINT8 tab_n[4] = { 3, 1, 3, 3 };
static const char *const tab_row[4][3] = { { "FLY INTO SPACE", "TALK TO BOLT", "STAY HERE" }, { "BOLT'S SHOP", 0, 0 }, { "HEAL", "TROPHIES", "MINING" }, { "SKILLS", "BAG", "SAVE GAME" } };
static const char *const tab_tip[4][3] = { { "TAKE OFF FOR SPACE", "WHAT HE THINKS", "CLOSE THIS MENU" }, { "POTIONS AND GEAR", 0, 0 }, { "FULL HEARTS + MANA", "YOUR FEATS", "MINE ORE FOR GOLD" }, { "SPEND SKILL POINTS", "LOOT, FOOD, WEAPONS", "WRITE TO YOUR SLOT" } };
static UINT8 hurt(void){ return sv.hp < hud_hpmax(sv.level) || sv.mp < hud_mpmax(sv.level); }
static void sm_draw(UINT8 tab, UINT8 sel, const char *msg){
    UINT8 i; char b[24], *d; const char *tip = tab_tip[tab][sel];
    txt_win(1);
    txt_line(1, 1, " GO SHOP CARE YOU", 18); txt_color(1, 1, 18, 1, TXT_GREY);
    if(skills_free()) txt_color(15, 1, 3, 1, TXT_GOLD);                  /* new skill points wait on the YOU tab */
    if(hurt()) txt_color(10, 1, 4, 1, TXT_RED);                          /* the CARE tab can heal you */
    if(tab == 0) txt_color(2, 1, 2, 1, TXT_GOLD); else if(tab == 1) txt_color(5, 1, 4, 1, TXT_GOLD); else if(tab == 2) txt_color(10, 1, 4, 1, TXT_GOLD); else txt_color(15, 1, 3, 1, TXT_GOLD);
    for(i = 0; i < 3; i++){
        if(i < tab_n[tab]){
            d = scat(b, i == sel ? S_CURSOR : " "); d = scat(d, tab_row[tab][i]);
            if(tab == TAB_YOU && i == 0 && skills_free()){ d = pad(d, b, 9); num(d, "(", skills_free(), " NEW)"); }
            if(tab == TAB_CARE && i == 0 && !hurt()){ d = pad(d, b, 9); scat(d, "(FULL)"); }
            txt_line(1, 2 + i, b, 18); txt_color(1, 2 + i, 18, 1, i == sel ? TXT_GOLD : TXT_WHITE);
        } else { txt_line(1, 2 + i, "", 18); }
    }
    txt_line(1, 5, msg ? msg : tip ? tip : "", 18); txt_color(1, 5, 18, 1, msg ? TXT_GOLD : TXT_GREY);
    txt_win(0);
}
static void sm_frame(UINT8 tab, UINT8 sel, const char *msg){ txt_win(1); txt_cls(); txt_box(0, 0, 20, SM_H); txt_win(0); sm_draw(tab, sel, msg); move_win(7, 144 - SM_H * 8); HIDE_SPRITES; SHOW_WIN; }   /* (no critters drawn over the box) */

/* start = tab * 4 + row (the planet re-opens the menu on the CARE tab after a full-screen page).
   Returns 0 closed, 1 fly into space, 2 the caller shows the FEATS page, 3 the caller runs MINING (both need the planet to repaint afterwards). */
UINT8 ship_menu(UINT8 start) BANKED {
    UINT8 tab = start >> 2, sel = start & 3, out = 0, rows, sprites = LCDC_REG & LCDCF_OBJON; const char *msg = 0;
    if(tab > 3 || sel >= tab_n[tab]){ tab = 0; sel = 0; }
    sm_frame(tab, sel, 0);
    for(;;){
        wait_vbl_done(); input_poll();
        rows = tab_n[tab];
        if(HIT(J_START)){ sfx(SFX_OK); out = 1; break; }              /* START takes off from any tab, like the GBA */
        if(HIT(J_B)) break;
        if(HIT(J_LEFT))  { tab = tab ? tab - 1 : 3; sel = 0; msg = 0; sfx(SFX_MOVE); sm_draw(tab, sel, msg); }
        if(HIT(J_RIGHT)) { tab = tab < 3 ? tab + 1 : 0; sel = 0; msg = 0; sfx(SFX_MOVE); sm_draw(tab, sel, msg); }
        if(HIT(J_UP) && rows > 1)   { sel = sel ? sel - 1 : rows - 1; msg = 0; sfx(SFX_MOVE); sm_draw(tab, sel, msg); }
        if(HIT(J_DOWN) && rows > 1) { sel = sel < rows - 1 ? sel + 1 : 0; msg = 0; sfx(SFX_MOVE); sm_draw(tab, sel, msg); }
        if(HIT(J_A)){
            sfx(SFX_OK);
            if(tab == TAB_GO){ if(sel == 1){ bolt_talk(); sm_frame(tab, sel, 0); continue; } if(sel == 0) out = 1; break; }
            if(tab == TAB_SHOP){ shop_run(0); sm_frame(tab, sel, 0); }
            else if(tab == TAB_CARE){
                if(sel == 0){
                    if(hurt()){ sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level); msg = "YOU ARE HEALED!"; sfx(SFX_FANFARE); } else msg = "ALREADY FULL";
                    sm_draw(tab, sel, msg);
                } else { out = sel + 1; break; }
            }
            else if(sel == 0){ skills_run(); sm_frame(tab, sel, 0); }
            else if(sel == 1){ loot_run(0); sm_frame(tab, sel, 0); }
            else { slot_write(cur_slot); msg = "SAVED!"; sfx(SFX_COIN); sm_draw(tab, sel, msg); }
        }
    }
    txt_win(1); txt_cls(); txt_win(0);
    if(sprites) SHOW_SPRITES;
    hud_redraw(); hud_pos(HUD_Y);
    return out;
}
