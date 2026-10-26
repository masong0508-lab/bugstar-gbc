#pragma bank 15
/* BUGSTAR GBC - upgrades. The ship's menu (FLY / SHOP / SKILLS / STAY), Bolt's shop and the SKILLS page.
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
#include "econ.h"

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
static UINT8 s_mode, s_n, s_first, s_disc;                   /* 0 the planet's shop (potions, bug gear), 1 the shipyard of a station (ship parts) */
static UINT16 s_price(UINT8 it, UINT8 lv){ return (UINT16)((UINT32)it_cost[it][it ? lv : 0] * (100 - s_disc) / 100); }
static void shop_draw(UINT8 cur, const char *msg){
    UINT8 i, it = s_first + cur, lv, r = 4 + s_n; char b[24], *d;
    txt_win(1);
    d = scat(b, s_mode ? "YARD  GOLD " : "SHOP  GOLD "); d = u32s(d, sv.gold > 999999999UL ? 999999999UL : sv.gold); pad(d, b, 18); txt_line(1, 1, b, 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    for(i = 0; i < s_n; i++){
        it = s_first + i; lv = it ? sv.up[it_up[it]] : sv.potions;
        d = scat(b, i == cur ? S_CURSOR : " "); d = scat(d, it_name[it]); d = pad(d, b, 8);
        if(it == I_POTION){ d = scat(d, "x"); d = u16s(d, lv); d = pad(d, b, 13); if(lv >= 9) scat(d, "FULL"); else num(d, "", POTION_COST, ""); }
        else { d = pips(d, lv, SHOP_MAXLV); if(lv < SHOP_MAXLV) num(d, "", s_price(it, lv), ""); else scat(d, "MAX"); }
        txt_line(1, 3 + i, b, 18); txt_color(1, 3 + i, 18, 1, i == cur ? TXT_GOLD : TXT_WHITE);
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
    txt_line(1, r + 3, msg ? msg : "", 18); txt_color(1, r + 3, 18, 1, TXT_RED);
    txt_win(0);
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
        if(HIT(J_A)){
            UINT8 it = s_first + cur, lv = it ? sv.up[it_up[it]] : sv.potions, mx = it ? SHOP_MAXLV : 9;
            if(lv >= mx){ msg = it ? "ALREADY THE BEST!" : "POCKETS FULL!"; sfx(SFX_BACK); }
            else if(sv.gold < s_price(it, lv)){ msg = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
            else {
                sv.gold -= s_price(it, lv); msg = "THANK YOU!"; sfx(SFX_COIN);
                if(!it) sv.potions++; else sv.up[it_up[it]]++;
                if(it == I_SHIELD) sv.mp = hud_mpmax(sv.level);
            }
            shop_draw(cur, msg);
        }
    }
    sfx(SFX_BACK);
    if(sprites) SHOW_SPRITES;
}

/* ---------------------------------------------------------------- the ship's menu */
static void menu_draw(UINT8 sel){
    UINT8 i; static const char *const it[4] = { "FLY INTO SPACE", "BOLT'S SHOP", "SKILLS", "STAY HERE" };
    char b[24], *d;
    txt_win(1);
    for(i = 0; i < 4; i++){ d = scat(b, i == sel ? S_CURSOR : " "); d = scat(d, it[i]);
        if(i == 2 && skills_free()){ d = pad(d, b, 9); num(d, "(", skills_free(), " NEW)"); }
        txt_line(1, 1 + i, b, 18); }
    txt_win(0);
}
static void menu_frame(UINT8 sel){ txt_win(1); txt_cls(); txt_box(0, 0, 20, 6); txt_win(0); menu_draw(sel); move_win(7, 144 - 48); SHOW_WIN; }

UINT8 ship_menu(void) BANKED {                                 /* 1 = fly into space */
    UINT8 sel = 0, out = 0;
    menu_frame(sel);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : 3; sfx(SFX_MOVE); menu_draw(sel); }
        if(HIT(J_DOWN)) { sel = sel < 3 ? sel + 1 : 0; sfx(SFX_MOVE); menu_draw(sel); }
        if(HIT(J_B)) break;
        if(HIT(J_A)){
            sfx(SFX_OK);
            if(sel == 0){ out = 1; break; }
            if(sel == 3) break;
            if(sel == 1) shop_run(0); else skills_run();
            menu_frame(sel);
        }
    }
    txt_win(1); txt_cls(); txt_win(0);
    hud_redraw(); hud_pos(HUD_Y);
    return out;
}
