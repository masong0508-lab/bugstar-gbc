#pragma bank 34
/* BUGSTAR GBC - the treasure bag (see loot.h). 31 kinds: 6 weapons, 5 armors, 4 charms (each worn in its own slot), MANA DEW, ELIXIR, the HP POTION (sv.potions), 5 foods, 8 oddities. Own SRAM block, keyed by slot + world seed. */
#include <gb/gb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "menus.h"
#include "econ.h"
#include "loot.h"

#define SQ_OFF 2048U                                      /* the bag's own block in the slot's SRAM bank (the Extra block has no spare bytes) */
enum { W0 = 0, A0 = 6, C0 = 11, L_MANA = 15, L_ELIXIR, L_POTION, L_BERRY, L_CAKE, L_STEW, L_NECTAR, L_JELLY, L_SHELL, L_COIN, L_DUST, L_SHARD, L_FOSSIL, L_BADGE, L_PEARL, L_GEAR, L_N };
#define K_W(i) ((i) < A0)
#define K_A(i) ((i) >= A0 && (i) < C0)
#define K_C(i) ((i) >= C0 && (i) < L_MANA)
#define K_G(i) ((i) < L_MANA)                              /* gear: can be worn */
static const char *const nm[L_N] = { "THORN", "FLINT FANG", "BEE STINGER", "AMBER SPIKE", "MOON CLAW", "STAR SABER",
    "LEAF VEST", "BARK PLATE", "BEETLE MAIL", "GEM MAIL", "STAR PLATE", "CLOVER", "LOCKET", "XP BEAD", "WARD CHARM",
    "MANA DEW", "ELIXIR", "HP POTION", "BERRY", "HONEY CAKE", "BUG STEW", "NECTAR", "JELLY",
    "SHINY SHELL", "ODD COIN", "MOTH DUST", "STAR SHARD", "FOSSIL", "BADGE", "GLOW PEARL", "OLD GEAR" };
static const char *const ds[L_N] = { "WEAPON: BITE +3", "WEAPON: BITE +6", "WEAPON: BITE +8", "WEAPON: BITE +10", "WEAPON: BITE +13", "WEAPON: BITE +17",
    "ARMOR: HITS -1", "ARMOR: HITS -2", "ARMOR: HITS -3", "ARMOR: HITS -4", "ARMOR: HITS -5",
    "CHARM: +15% GOLD", "CHARM: HEALS SLOWLY", "CHARM: +25% XP", "CHARM: HITS -1",
    "MANA BACK: HALF", "FULL HP AND MANA", "HEALS HALF YOUR HP", "HEALS A LITTLE HP", "HEALS A THIRD OF HP", "HEALS MOST OF YOUR HP", "HP HALF, MANA QUARTER", "FULL HP",
    "A SHINY THING. SELL IT", "AN ODD COIN. SELL IT", "GLOWING DUST. SELL IT", "FELL FROM A STAR. SELL", "OLD BUG BONES. SELL IT", "A PIRATE'S. SELL IT", "GLOWS FAINTLY. SELL IT", "OLD MACHINE PART. SELL" };
static const UINT16 val[L_N] = { 40, 120, 200, 320, 560, 900,  60, 150, 340, 620, 1000,  200, 260, 240, 300,
    18, 150, 20, 3, 9, 16, 30, 70,  25, 60, 110, 240, 80, 150, 180, 320 };
static const UINT8 pw[L_MANA] = { 3, 6, 8, 10, 13, 17,  1, 2, 3, 4, 5,  0, 0, 0, 0 };   /* weapon: bite, armor: hits */
typedef struct { UINT32 seed; UINT8 own[3]; UINT8 eq[3]; UINT8 n[15]; UINT8 chk; } Bag;    /* own/eq: weapons, armor, charms (eq = index in the group + 1) */
static Bag b;
static UINT8 b_slot = 255; static UINT32 b_seed;
static UINT16 rng;

static UINT8 sumb(void){ UINT8 *p = (UINT8 *)&b, i, c = 0x5A; for(i = 0; i < sizeof b - 1; i++) c += p[i] * 3 + i; return c; }
static void bag_save(void){
    UINT8 *p = (UINT8 *)&b, i; b.chk = sumb();
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof b; i++) *(volatile UINT8 *)(0xA000U + SQ_OFF + i) = p[i];
    DISABLE_RAM;
}
static void ensure(void){
    UINT8 *p = (UINT8 *)&b, i;
    if(b_slot == cur_slot && b_seed == sv.seed) return;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof b; i++) p[i] = *(volatile UINT8 *)(0xA000U + SQ_OFF + i);
    DISABLE_RAM;
    if(b.seed != sv.seed || b.chk != sumb()){ for(i = 0; i < sizeof b; i++) p[i] = 0; b.seed = sv.seed; }
    b_slot = cur_slot; b_seed = sv.seed;
}
static UINT8 rnd(UINT8 n){ if(!rng) rng = (UINT16)frames32() | 1; rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; rng ^= (UINT16)frames32(); if(!rng) rng = 1; return (UINT8)(((UINT16)(UINT8)rng * n) >> 8); }
static UINT8 grp(UINT8 i){ return K_W(i) ? 0 : K_A(i) ? 1 : 2; }
static UINT8 gi(UINT8 i){ return i - (K_W(i) ? W0 : K_A(i) ? A0 : C0); }
static UINT8 have(UINT8 i){
    if(K_G(i)) return (b.own[grp(i)] >> gi(i)) & 1;
    if(i == L_POTION) return sv.potions;
    return b.n[i - L_MANA - (i > L_POTION ? 1 : 0)];
}
static UINT8 *slot(UINT8 i){ return i == L_POTION ? &sv.potions : &b.n[i - L_MANA - (i > L_POTION ? 1 : 0)]; }
static UINT8 worn(UINT8 i){ return K_G(i) && b.eq[grp(i)] == gi(i) + 1; }
static UINT8 give(UINT8 i){                                /* 1 if it went in (gear you own, or a full stack, does not) */
    ensure();
    if(K_G(i)){ if(have(i)) return 0; b.own[grp(i)] |= 1 << gi(i); bag_save(); return 1; }
    { UINT8 *p = slot(i); if(*p >= (i == L_POTION ? 9 : 99)) return 0; (*p)++; bag_save(); return 1; }
}
static UINT8 wearing(UINT8 g){ UINT8 e = b.eq[g]; return e ? e - 1 : 255; }
UINT8 loot_bite(void) BANKED { UINT8 e; ensure(); e = wearing(0); return e < 6 ? pw[e] : 0; }
UINT8 loot_armor(void) BANKED { UINT8 e, a = 0; ensure(); e = wearing(1); if(e < 5) a = pw[A0 + e]; if(wearing(2) == 3) a++; return a; }
UINT16 loot_clover(UINT16 g) BANKED { ensure(); return wearing(2) == 0 ? g * 15 / 100 : 0; }
UINT8 loot_xp(UINT8 x) BANKED { ensure(); return wearing(2) == 2 ? (x >> 2) + 1 : 0; }
#define HUNGRY_AT 120                                      /* ticks of ~2 s: about 4 minutes in a dungeon, then 1 HP every 4 s until you eat (never to 0) */
static UINT8 hg;                                           /* hunger: not saved, starts full each time the game is loaded */
static void fed(UINT8 n){ hg = hg > n ? hg - n : 0; }
UINT8 loot_regen(void) BANKED {
    UINT16 mx = hud_hpmax(sv.level); UINT8 ret = 0; ensure();
    if(wearing(2) == 1 && sv.hp && sv.hp < mx){ sv.hp += 1 + mx / 40; if(sv.hp > mx) sv.hp = mx; }
    if(opt.hunger && sv.hp){
        if(hg < 255) hg++;
        if(hg == HUNGRY_AT) ret = 1;
        if(hg > HUNGRY_AT && !(hg & 1) && sv.hp > 1) sv.hp--;
    }
    return ret;
}

/* what falls where: depth 1..6 */
UINT8 loot_chest(UINT8 depth, char *msg) BANKED {
    UINT8 r = rnd(100), it, t; char *d;
    if(r >= 60) return 0;                                    /* 4 chests in 10 hold only gold */
    t = rnd(100);
    if(t < 10 + depth * 2){                                  /* gear: better ones deeper */
        UINT8 tier = depth - 1 + rnd(2), g = rnd(3);
        if(g == 0) it = W0 + (tier > 5 ? 5 : tier);
        else if(g == 1) it = A0 + (tier > 4 ? 4 : tier);
        else it = C0 + rnd(4);
        if(have(it)) it = L_MANA;
    } else if(t < 40){ it = L_BERRY + (rnd(10) < 2 + depth ? 1 : 0) + (depth > 2 && rnd(3) == 0 ? 1 : 0); if(depth > 4 && rnd(4) == 0) it = L_NECTAR + rnd(2); }
    else if(t < 55) it = (depth > 3 && rnd(10) == 0) ? L_ELIXIR : L_MANA;
    else it = L_SHELL + rnd(depth > 3 ? 8 : 5);
    if(!give(it)) return 0;
    sfx(SFX_FANFARE);
    d = scat(msg, "THE CHEST HOLDS GOLD AND A "); d = scat(d, nm[it]); scat(d, K_G(it) ? "!" : ".");
    return 1;
}
void loot_kill(UINT8 kind, UINT8 depth) BANKED {
    UINT8 r = rnd(100);
    if(r >= 14 + kind) return;                                /* about 1 kill in 7 */
    if(give(r < 8 ? L_BERRY : (r < 11 ? L_SHELL : (depth > 2 && r < 13) ? L_FOSSIL : L_COIN))) sfx(SFX_PICK);
}
void loot_odd(void) BANKED { UINT8 r = rnd(100); if(r < 25 && give(r < 8 ? L_BADGE : r < 14 ? L_GEAR : r < 20 ? L_COIN : L_DUST)) sfx(SFX_PICK); }

/* ---------------------------------------------------------------- eating, drinking, wearing */
static UINT8 heal(UINT16 n){ UINT16 mx = hud_hpmax(sv.level); if(sv.hp >= mx) return 0; sv.hp = sv.hp + n > mx ? mx : sv.hp + n; return 1; }
static UINT8 use(UINT8 i, char *msg){                      /* 1 if it was used up */
    UINT16 mx = hud_hpmax(sv.level), mm = hud_mpmax(sv.level);
    if(K_G(i)){ UINT8 g = grp(i); b.eq[g] = b.eq[g] == gi(i) + 1 ? 0 : gi(i) + 1; bag_save(); sfx(SFX_OK); scat(msg, b.eq[g] ? "WORN NOW" : "TAKEN OFF"); return 0; }
    if(i == L_MANA){ if(sv.mp >= mm){ scat(msg, "MANA IS FULL"); return 0; } sv.mp = sv.mp + mm / 2 > mm ? mm : sv.mp + mm / 2; (*slot(i))--; sfx(SFX_SPARK); scat(msg, "MANA RESTORED"); bag_save(); return 1; }
    if(i == L_ELIXIR){ if(sv.hp >= mx && sv.mp >= mm){ scat(msg, "ALL FULL"); return 0; } sv.hp = mx; sv.mp = mm; (*slot(i))--; sfx(SFX_FANFARE); scat(msg, "FULL HP AND MANA!"); bag_save(); return 1; }
    if(i == L_POTION){ if(!heal(mx / 2 + mx / 20 * UPG(SK_VIGOR))){ scat(msg, "HP IS FULL"); return 0; } sv.potions--; sfx(SFX_SPARK); scat(msg, "GLUG GLUG. BETTER."); return 1; }
    if(i >= L_BERRY && i <= L_JELLY){
        if(!heal(i == L_BERRY ? mx / 8 + 3 : i == L_CAKE ? mx / 3 : i == L_STEW ? mx / 5 * 3 : i == L_NECTAR ? mx / 2 : mx) && !(opt.hunger && hg >= 30)){ scat(msg, "HP IS FULL"); return 0; }
        fed(i == L_BERRY ? 60 : i == L_CAKE ? 120 : 255);
        if(i == L_NECTAR){ sv.mp = sv.mp + mm / 4 > mm ? mm : sv.mp + mm / 4; }
        (*slot(i))--; sfx(SFX_SPARK); scat(msg, "YUM!"); bag_save(); return 1;
    }
    scat(msg, "NOT FOR EATING. SELL IT"); sfx(SFX_BACK); return 0;
}
UINT8 loot_eat(char *msg) BANKED {
    UINT8 i; ensure(); msg[0] = 0;
    if(sv.hp >= hud_hpmax(sv.level) && !(opt.hunger && hg >= 30)){ scat(msg, "NOT HUNGRY (HP FULL)"); return 0; }
    for(i = L_BERRY; i <= L_STEW; i++) if(have(i)) return use(i, msg);
    scat(msg, "NO FOOD IN THE BAG"); return 0;
}

/* ---------------------------------------------------------------- the page (bag / sell) */
static const UINT8 stock[8] = { 0, 1, 2, 6, 7, 8, 11, 12 };    /* gear the shops sell (mode 3): the cheap half, so the deep finds stay special */
static UINT16 price(UINT8 i, UINT8 mode){ UINT16 p = mode == 3 ? val[i] * 2 : (UINT16)((UINT32)val[i] * (mode == 2 ? 60 + fame_disc() : 50) / 100); return p ? p : 1; }
static UINT8 list[L_N];
static UINT8 u_it, u_k; static UINT32 u_g;                  /* BUYBACK: the last sale of this visit (LEFT undoes it) */
static UINT8 build(UINT8 mode){ UINT8 i, n = 0; if(mode == 3){ for(i = 0; i < 8; i++) if(!have(stock[i])) list[n++] = stock[i]; return n; } for(i = 0; i < L_N; i++) if(have(i)) list[n++] = i; return n; }
static void put(const char *d, UINT8 x, UINT8 y, UINT8 col){ txt_line(x, y, d, 18); txt_color(x, y, 18, 1, col); }
static void draw(UINT8 mode, UINT8 n, UINT8 cur, UINT8 top, const char *msg){
    UINT8 i, r; char t[24], *d;
    txt_win(1);
    d = scat(t, mode == 3 ? "GEAR  GOLD " : mode ? "SELL  GOLD " : "BAG   GOLD "); d = u32s(d, sv.gold > 999999999UL ? 999999999UL : sv.gold); put(t, 1, 1, TXT_GOLD);
    if(mode == 3) scat(t, "A:BUY  B:BACK");
    else if(mode){ d = scat(t, "PAYS "); d = u16s(d, mode == 2 ? 60 + fame_disc() : 50); scat(d, "% OF VALUE"); } else scat(t, "A:USE/WEAR  B:BACK");
    put(t, 1, 2, TXT_GREY);
    for(r = 0; r < 8; r++){
        i = top + r;
        if(i < n){
            UINT8 it = list[i]; d = scat(t, i == cur ? S_CURSOR : " "); d = scat(d, nm[it]); while((UINT8)(d - t) < 12) *d++ = ' '; *d = 0;
            if(mode == 3) d = scat(d, "    "); else if(K_G(it)) d = scat(d, worn(it) ? "WORN" : "    "); else { d = scat(d, "x"); d = u16s(d, have(it)); }
            while((UINT8)(d - t) < 16) *d++ = ' '; *d = 0;
            if(mode) u16s(d, price(it, mode)); else if(it < C0){ d = scat(d, it < A0 ? "+" : "-"); u16s(d, pw[it]); }
            if(mode == 3 && sv.gold < price(it, 3)) txt_color(16, 4 + r, 2, 1, TXT_RED);
            put(t, 1, 4 + r, i == cur ? TXT_GOLD : (K_G(list[i]) && worn(list[i])) ? TXT_GREY : TXT_WHITE);
        } else put("", 1, 4 + r, TXT_WHITE);
    }
    if(!n && mode == 3){ put("SOLD OUT! YOU OWN", 1, 4, TXT_GREY); put("EVERYTHING WE HAVE.", 1, 5, TXT_GREY); }
    else if(!n){ put("NOTHING IN THE BAG", 1, 4, TXT_GREY); put("OPEN CHESTS, BITE BUGS,", 1, 5, TXT_GREY); put("SHOOT PIRATES.", 1, 6, TXT_GREY); }
    put(n ? ds[list[cur]] : "", 1, 13, TXT_WHITE);
    put(msg ? msg : "", 1, 14, TXT_GOLD);
    if(!mode && opt.hunger){ put(hg >= HUNGRY_AT ? "BELLY: STARVING!" : hg >= 80 ? "BELLY: HUNGRY" : hg >= 30 ? "BELLY: PECKISH" : "BELLY: FULL", 1, 15, hg >= HUNGRY_AT ? TXT_RED : TXT_GREY); }
    if(mode && mode < 3) put(u_k ? "LEFT:UNDO SALE" : "", 1, 15, TXT_GREY);
    put(mode == 3 ? "" : mode ? "A:SELL ONE SEL:ALL" : "SELECT+DOWN EATS", 1, 16, TXT_GREY);
    txt_win(0);
}
void loot_run(UINT8 mode) BANKED {
    UINT8 n, cur = 0, top = 0, sprites = LCDC_REG & LCDCF_OBJON, it; char m[28]; const char *msg = 0;
    ensure(); u_k = 0;
    HIDE_SPRITES;
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 18); txt_win(0);
    n = build(mode); draw(mode, n, cur, top, msg); move_win(7, 0); SHOW_WIN;
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) break;
        if(n && HIT(J_UP))   { cur = cur ? cur - 1 : n - 1; msg = 0; sfx(SFX_MOVE); }
        else if(n && HIT(J_DOWN)) { cur = cur < n - 1 ? cur + 1 : 0; msg = 0; sfx(SFX_MOVE); }
        else if(mode && u_k && HIT(J_LEFT)){                  /* buy the last sale back at the price it fetched */
            m[0] = 0; msg = m;
            if(sv.gold < u_g){ scat(m, "NOT ENOUGH GOLD"); sfx(SFX_BACK); }
            else {
                if(K_G(u_it)) b.own[grp(u_it)] |= 1 << gi(u_it); else *slot(u_it) += u_k;
                sv.gold -= u_g; u_k = 0; bag_save(); sfx(SFX_OK); scat(m, "BOUGHT BACK");
            }
        }
        else if(n && (HIT(J_A) || (mode && mode < 3 && HIT(J_SELECT)))){
            it = list[cur]; m[0] = 0; msg = m;
            if(mode == 3){
                UINT16 pr = price(it, 3);
                if(!HIT(J_A)){} else if(sv.gold < pr){ scat(m, "NOT ENOUGH GOLD"); sfx(SFX_BACK); }
                else if(give(it)){ sv.gold -= pr; sfx(SFX_COIN); scat(m, "BOUGHT! WEAR IT"); }
                else { scat(m, "YOU HAVE IT"); sfx(SFX_BACK); }
            } else
            if(!mode){ if(HIT(J_A)) use(it, m); }
            else if(K_G(it) && worn(it)){ scat(m, "TAKE IT OFF FIRST!"); sfx(SFX_BACK); }
            else {
                UINT8 k = (HIT(J_SELECT) && !K_G(it)) ? have(it) : 1, j; UINT32 g = 0;
                for(j = 0; j < k; j++){ g += price(it, mode); if(K_G(it)) b.own[grp(it)] &= ~(1 << gi(it)); else (*slot(it))--; }
                u_it = it; u_k = k; u_g = g;
                sv.gold += g; bag_save(); sfx(SFX_COIN); scat(m, "SOLD FOR "); u32s(m + 9, g);
            }
        } else continue;
        n = build(mode); if(cur >= n) cur = n ? n - 1 : 0;
        if(cur < top) top = cur; if(cur > top + 7) top = cur - 7;
        draw(mode, n, cur, top, msg);
    }
    sfx(SFX_BACK);
    txt_win(1); txt_cls(); txt_win(0);
    if(sprites) SHOW_SPRITES;
}
