#pragma bank 34
/* BUGSTAR GBC - the treasure bag (see loot.h). 13 kinds: 4 weapons, MANA DEW, the HP POTION (sv.potions), 3 foods, 4 oddities. */
#include <gb/gb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "menus.h"
#include "econ.h"
#include "loot.h"

enum { L_THORN, L_FLINT, L_AMBER, L_SABER, L_MANA, L_POTION, L_BERRY, L_CAKE, L_STEW, L_SHELL, L_COIN, L_DUST, L_SHARD, L_N };
#define L_NW 4                                            /* the first 4 are weapons */
static const char *const nm[L_N] = { "THORN", "FLINT FANG", "AMBER SPIKE", "STAR SABER", "MANA DEW", "HP POTION", "BERRY", "HONEY CAKE", "BUG STEW", "SHINY SHELL", "ODD COIN", "MOTH DUST", "STAR SHARD" };
static const char *const ds[L_N] = { "WEAPON: BITE +3", "WEAPON: BITE +6", "WEAPON: BITE +10", "WEAPON: BITE +15", "MANA BACK: HALF", "HEALS HALF YOUR HP", "HEALS A LITTLE HP", "HEALS A THIRD OF HP", "HEALS MOST OF YOUR HP", "A SHINY THING. SELL IT", "AN ODD COIN. SELL IT", "GLOWING DUST. SELL IT", "FELL FROM A STAR. SELL IT" };
static const UINT16 val[L_N] = { 40, 120, 320, 800, 18, 20, 3, 9, 16, 25, 60, 110, 240 };
static const UINT8  wpn[L_NW] = { 3, 6, 10, 15 };
static UINT16 rng;

static UINT8 rnd(UINT8 n){ if(!rng) rng = (UINT16)frames32() | 1; rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; rng ^= (UINT16)frames32(); if(!rng) rng = 1; return (UINT8)(((UINT16)(UINT8)rng * n) >> 8); }
static UINT8 have(UINT8 i){
    if(i < L_NW) return (ex.rsv[0] >> i) & 1;
    if(i == L_MANA) return ex.rsv[2];
    if(i == L_POTION) return sv.potions;
    return i < L_SHELL ? ex.rsv[3 + i - L_BERRY] : ex.rsv[6 + i - L_SHELL];
}
static UINT8 *slot(UINT8 i){ return i == L_MANA ? &ex.rsv[2] : i == L_POTION ? &sv.potions : i < L_SHELL ? &ex.rsv[3 + i - L_BERRY] : &ex.rsv[6 + i - L_SHELL]; }
static UINT8 give(UINT8 i){                                /* 1 if it went in (a weapon you own, or a full stack of 99, does not) */
    if(i < L_NW){ if(have(i)) return 0; ex.rsv[0] |= 1 << i; return 1; }
    { UINT8 *p = slot(i); if(*p >= (i == L_POTION ? 9 : 99)) return 0; (*p)++; return 1; }
}
UINT8 loot_bite(void) BANKED { UINT8 e = ex.rsv[1]; return e && e <= L_NW && have(e - 1) ? wpn[e - 1] : 0; }

/* what falls where: depth 1..6 */
UINT8 loot_chest(UINT8 depth, char *msg) BANKED {
    UINT8 r = rnd(100), it; char *d;
    if(r >= 60) return 0;                                    /* 4 chests in 10 hold only gold */
    if(r < 6 + depth * 2){                                   /* a weapon: better ones deeper */
        it = (UINT8)(rnd(3) + (depth > 3 ? 1 : 0)); if(it > 3) it = 3;
        if(have(it)) it = L_MANA;                            /* you own it: a potion instead */
    } else if(r < 30) it = L_BERRY + (UINT8)(rnd(10) < (UINT8)(2 + depth) ? 1 : 0) + (depth > 3 && rnd(3) == 0 ? 1 : 0);
    else if(r < 40) it = L_MANA;
    else it = L_SHELL + rnd(depth > 3 ? 4 : 3);
    if(!give(it)) return 0;
    sfx(SFX_FANFARE);
    d = scat(msg, "THE CHEST HOLDS GOLD AND A "); d = scat(d, nm[it]); scat(d, it < L_NW ? "!" : ".");
    return 1;
}
void loot_kill(UINT8 kind, UINT8 depth) BANKED {
    UINT8 r = rnd(100);
    if(r >= 14 + kind) return;                                /* about 1 kill in 7 */
    if(give(r < 8 ? L_BERRY : (r < 11 ? L_SHELL : L_COIN))) sfx(SFX_PICK);
}
void loot_odd(void) BANKED { if(rnd(4) == 0 && give(rnd(2) ? L_COIN : L_DUST)) sfx(SFX_PICK); }

/* ---------------------------------------------------------------- eating and drinking */
static UINT8 heal(UINT16 n){ UINT16 mx = hud_hpmax(sv.level); if(sv.hp >= mx) return 0; sv.hp = sv.hp + n > mx ? mx : sv.hp + n; return 1; }
static UINT8 use(UINT8 i, char *msg){                      /* 1 if it was used up */
    UINT16 mx = hud_hpmax(sv.level);
    if(i < L_NW){ ex.rsv[1] = ex.rsv[1] == i + 1 ? 0 : i + 1; sfx(SFX_OK); scat(msg, ex.rsv[1] ? "IN YOUR MOUTH NOW" : "BARE FANGS AGAIN"); return 0; }
    if(i == L_MANA){ UINT16 m = hud_mpmax(sv.level); if(sv.mp >= m){ scat(msg, "MANA IS FULL"); return 0; } sv.mp = sv.mp + m / 2 > m ? m : sv.mp + m / 2; (*slot(i))--; sfx(SFX_SPARK); scat(msg, "MANA RESTORED"); return 1; }
    if(i == L_POTION){ if(!heal(mx / 2 + mx / 20 * UPG(SK_VIGOR))){ scat(msg, "HP IS FULL"); return 0; } sv.potions--; sfx(SFX_SPARK); scat(msg, "GLUG GLUG. BETTER."); return 1; }
    if(i >= L_BERRY && i <= L_STEW){
        if(!heal(i == L_BERRY ? mx / 8 + 3 : i == L_CAKE ? mx / 3 : mx / 5 * 3)){ scat(msg, "HP IS FULL"); return 0; }
        (*slot(i))--; sfx(SFX_SPARK); scat(msg, "YUM!"); return 1;
    }
    scat(msg, "NOT FOR EATING. SELL IT"); sfx(SFX_BACK); return 0;
}
UINT8 loot_eat(char *msg) BANKED {
    UINT8 i; msg[0] = 0;
    if(sv.hp >= hud_hpmax(sv.level)){ scat(msg, "NOT HUNGRY (HP FULL)"); return 0; }
    for(i = L_BERRY; i <= L_STEW; i++) if(have(i)) return use(i, msg);
    scat(msg, "NO FOOD IN THE BAG"); return 0;
}

/* ---------------------------------------------------------------- the page (bag / sell) */
static UINT16 price(UINT8 i, UINT8 mode){ UINT16 p = (UINT16)((UINT32)val[i] * (mode == 2 ? 60 + fame_disc() : 50) / 100); return p ? p : 1; }
static UINT8 list[L_N];
static UINT8 build(void){ UINT8 i, n = 0; for(i = 0; i < L_N; i++) if(have(i)) list[n++] = i; return n; }
static void put(const char *d, UINT8 x, UINT8 y, UINT8 col){ txt_line(x, y, d, 18); txt_color(x, y, 18, 1, col); }
static void draw(UINT8 mode, UINT8 n, UINT8 cur, UINT8 top, const char *msg){
    UINT8 i, r; char b[24], *d;
    txt_win(1);
    d = scat(b, mode ? "SELL  GOLD " : "BAG   GOLD "); d = u32s(d, sv.gold > 999999999UL ? 999999999UL : sv.gold); put(b, 1, 1, TXT_GOLD);
    if(mode){ d = scat(b, "PAYS "); d = u16s(d, mode == 2 ? 60 + fame_disc() : 50); scat(d, "% OF VALUE"); } else scat(b, "A:USE/EQUIP  B:BACK");
    put(b, 1, 2, TXT_GREY);
    for(r = 0; r < 8; r++){
        i = top + r;
        if(i < n){
            UINT8 it = list[i]; d = scat(b, i == cur ? S_CURSOR : " "); d = scat(d, nm[it]); while((UINT8)(d - b) < 12) *d++ = ' '; *d = 0;
            if(it < L_NW){ d = scat(d, ex.rsv[1] == it + 1 ? "WORN" : "    "); } else { d = scat(d, "x"); d = u16s(d, have(it)); }
            while((UINT8)(d - b) < 15) *d++ = ' '; *d = 0;
            if(mode) u16s(d, price(it, mode)); else if(it < L_NW){ d = scat(d, "+"); u16s(d, wpn[it]); }
            put(b, 1, 4 + r, i == cur ? TXT_GOLD : TXT_WHITE);
        } else put("", 1, 4 + r, TXT_WHITE);
    }
    if(!n){ put("NOTHING IN THE BAG", 1, 4, TXT_GREY); put("OPEN CHESTS, BITE BUGS,", 1, 5, TXT_GREY); put("SHOOT PIRATES.", 1, 6, TXT_GREY); }
    put(n ? ds[list[cur]] : "", 1, 13, TXT_WHITE);
    put(msg ? msg : "", 1, 14, TXT_GOLD);
    put(mode ? "A:SELL ONE SEL:ALL" : "SELECT+DOWN EATS", 1, 16, TXT_GREY);
    txt_win(0);
}
void loot_run(UINT8 mode) BANKED {
    UINT8 n, cur = 0, top = 0, sprites = LCDC_REG & LCDCF_OBJON, it; char m[28]; const char *msg = 0;
    HIDE_SPRITES;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 18); txt_win(0);
    n = build(); draw(mode, n, cur, top, msg); move_win(7, 0); SHOW_WIN;
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) break;
        if(n && HIT(J_UP))   { cur = cur ? cur - 1 : n - 1; msg = 0; sfx(SFX_MOVE); }
        else if(n && HIT(J_DOWN)) { cur = cur < n - 1 ? cur + 1 : 0; msg = 0; sfx(SFX_MOVE); }
        else if(n && (HIT(J_A) || (mode && HIT(J_SELECT)))){
            it = list[cur]; m[0] = 0; msg = m;
            if(!mode){ if(HIT(J_A)) use(it, m); }
            else if(it < L_NW && ex.rsv[1] == it + 1){ scat(m, "TAKE IT OUT OF YOUR MOUTH!"); sfx(SFX_BACK); }
            else {
                UINT8 k = (HIT(J_SELECT) && it >= L_NW) ? have(it) : 1, j; UINT32 g = 0;
                for(j = 0; j < k; j++){ g += price(it, mode); if(it < L_NW) ex.rsv[0] &= ~(1 << it); else (*slot(it))--; }
                sv.gold += g; sfx(SFX_COIN); scat(m, "SOLD FOR "); u32s(m + 9, g);
            }
        } else continue;
        n = build(); if(cur >= n) cur = n ? n - 1 : 0;
        if(cur < top) top = cur; if(cur > top + 7) top = cur - 7;
        draw(mode, n, cur, top, msg);
    }
    sfx(SFX_BACK);
    txt_win(1); txt_cls(); txt_win(0);
    if(sprites) SHOW_SPRITES;
}
