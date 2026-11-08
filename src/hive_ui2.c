#pragma bank 51
/* BUGSTAR GBC - BUG HIVE, more pages (hive_int.h): trade, outposts, the link to the game, score and achievements, options. The same list page as hive_ui.c. */
#include <gb/gb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "econ.h"
#include "hive_int.h"

static const char *const rname[R_N] = { "FOOD", "LEAF", "ORE", "BARS", "CRYSTAL", "HONEY" };
static const char *const aname[16] = { "FIRST BROOD", "TEN BUGS", "A BIG HIVE", "RAID WON", "BOSS SLAIN", "FIRST BAR", "FIRST CRYSTAL", "FIRST RESEARCH", "ALL RESEARCH", "TEN DAYS", "A WHOLE YEAR",
    "LEVEL 5 ROOM", "ALL ROOMS BUILT", "FIVE QUESTS", "AN OUTPOST", "ROYAL VISIT" };
static const char *const dname[3] = { "CALM", "NORMAL", "HARSH" };
static const char *const tname[NTECH] = { "TRAILS", "COMPOST", "SMELTING", "MARKETS", "SPEARS", "MASONRY", "ARCHERY", "ALARM", "DEEP DIG", "BREEDING", "STAR CHART", "ASCENSION" };
static void pad(char *d, UINT8 n){ UINT8 l = 0; while(d[l]) l++; while(l < n) d[l++] = ' '; d[l] = 0; }
static UINT8 has_tech(UINT8 t){ return (HV->tech >> t) & 1; }
static void page_open(void){ txt_win(1); WX_REG = 7; WY_REG = 0; SHOW_WIN; txt_pal_reset(); }
static void frame_page(const char *title){
    txt_box(0, 0, 20, 18); txt_line(2, 1, title, 16); txt_color(2, 1, 16, 1, TXT_GOLD);
}
typedef UINT8 (*LineFn)(UINT8 i, char *o);                          /* writes the line, returns its colour */
typedef void (*FootFn)(UINT8 i, char *l1, char *l2);
static UINT8 lp_dir, lp_sel;
/* returns the item (A), 255 (B), 254 (left or right: lp_dir 1 / 2), 253 (SELECT) */
static UINT8 listpage(const char *title, UINT8 n, LineFn lf, FootFn ff, UINT8 hint){
    UINT8 top = 0, f = 0, i, col, redraw = 1; char b[28], l1[24], l2[24];
    (void)hint;
    if(lp_sel >= n) lp_sel = n ? n - 1 : 0;
    for(;;){
        if(lp_sel < top) top = lp_sel; if(lp_sel >= top + 10) top = lp_sel - 9;
        if(redraw){
            txt_soft(); frame_page(title);
            for(i = 0; i < 10 && top + i < n; i++){
                b[0] = top + i == lp_sel ? T_CURSOR : ' '; b[1] = 0; col = lf(top + i, b + 1); txt_line(1, 3 + i, b, 18);
                txt_color(1, 3 + i, 18, 1, top + i == lp_sel ? TXT_GOLD : col);
            }
            if(top) txt_put(18, 2, S_ARROW_UP); if(top + 10 < n) txt_put(18, 13, S_ARROW_DN);
            l1[0] = l2[0] = 0; if(ff) ff(lp_sel, l1, l2);
            txt_line(1, 14, l1, 18); txt_line(1, 15, l2, 18); txt_color(1, 14, 18, 2, TXT_GREY);
            txt_soft_end(); redraw = 0;
        }
        wait_vbl_done(); input_poll(); f++;
        if(HITR(J_UP) && n){ lp_sel = lp_sel ? lp_sel - 1 : n - 1; redraw = 1; sfx(SFX_MOVE); }
        if(HITR(J_DOWN) && n){ lp_sel = lp_sel + 1 < n ? lp_sel + 1 : 0; redraw = 1; sfx(SFX_MOVE); }
        if(HIT(J_LEFT)){ lp_dir = 2; return 254; }
        if(HIT(J_RIGHT)){ lp_dir = 1; return 254; }
        if(HIT(J_SELECT)) return 253;
        if(HIT(J_A) && n) return lp_sel;
        if(HIT(J_B)){ sfx(SFX_BACK); return 255; }
    }
}
/* ---------------------------------------------------------------- trade */
static UINT8 town_star(void){
    UINT8 s, n = 0, k; UINT16 want;
    for(s = 0; s < 255; s++) if(ex.city[s >> 3] & (1 << (s & 7))) n++;
    if(!n){ for(s = 0; s < 255; s++) if(ex.seen[s >> 3] & (1 << (s & 7))) n++; if(!n) return sv.star; k = 1; }
    else k = 0;
    want = ((UINT16)HV->town + HV->day / 3) % n;
    for(s = 0; s < 255; s++){
        if(k ? (ex.seen[s >> 3] & (1 << (s & 7))) : (ex.city[s >> 3] & (1 << (s & 7)))){ if(!want) return s; want--; }
    }
    return sv.star;
}
static UINT16 price(UINT8 r, UINT8 sell){
    static const UINT8 base[R_N] = { 3, 3, 5, 12, 25, 10 };
    static const UINT8 pr[R_N] = { 0, 1, 2, 2, 3, 4 };
    UINT8 tk = world_kind(world_seed(town_star())); UINT16 m = 130 - 14 * (UINT16)hs_prof_at(tk, pr[r]), p;
    p = base[r] * m / 100; if(sell){ p = p * 7 / 10; p += p * HV->bsum[C_MARKET] / 25; if(HV->trader) p += p / 5; }
    return p ? p : 1;
}
static UINT8 trl(UINT8 i, char *o){
    static const char *const sn[R_N] = { "FOOD", "LEAF", "ORE", "BARS", "CRYS", "HONEY" };
    char *d = scat(o, sn[i]); while(d - o < 6) *d++ = ' ';
    d = u16s(d, HV->res[i]); while(d - o < 10) *d++ = ' ';
    d = scat(d, "B"); d = u16s(d, price(i, 0)); d = scat(d, " S"); u16s(d, price(i, 1));
    return TXT_WHITE;
}
static void trf(UINT8 i, char *l1, char *l2){
    char *d = scat(l1, "TOWN "); d = world_name(d, world_seed(town_star())); d = scat(d, " "); (void)i;
    scat(d, HV->trader ? "TRADERS!" : "");
    scat(l2, "R:BUY 5  L:SELL 5");
}
void hu2_trade(void) BANKED {
    UINT8 r;
    page_open(); lp_sel = 0;
    if(!HV->bcnt[C_MARKET]){
        txt_soft(); frame_page("TRADE"); txt_put(2, 4, "TOWNS NEED A"); txt_put(2, 5, "MARKET."); txt_put(2, 7, "RESEARCH MARKETS,"); txt_put(2, 8, "THEN BUILD ONE."); txt_soft_end();
        for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_B) || HIT(J_A)) break; }
        return;
    }
    for(;;){
        char b[24]; UINT16 p;
        r = listpage("TRADE (SHELLS)", R_N, trl, trf, 0);
        (void)b;
        if(r == 255) break;
        if(r == 254){
            if(lp_dir == 1){ p = price(lp_sel, 0) * 5; if(HV->shells >= p){ UINT16 cap = hs_stock(); if(HV->res[lp_sel] + 5 <= cap){ HV->shells -= p; HV->res[lp_sel] += 5; sfx(SFX_COIN); } else sfx(SFX_HURT); } else sfx(SFX_HURT); }
            else { if(HV->res[lp_sel] >= 5){ HV->res[lp_sel] -= 5; HV->shells += price(lp_sel, 1) * 5; if(HV->shells > 9999) HV->shells = 9999; sfx(SFX_COIN); } else sfx(SFX_HURT); }
        }
    }
}
/* ---------------------------------------------------------------- outposts */
static UINT16 okm;
static UINT8 ol(UINT8 i, char *o){
    char *d = scat(o, kind_name[i]); UINT8 k; while(d - o < 8) *d++ = ' ';
    for(k = 0; k < 5; k++) *d++ = hs_prof_at(i, k) >= 3 ? '*' : hs_prof_at(i, k) >= 1 ? '-' : '.';
    d = scat(d, " L"); d = u16s(d, HV->outp[i]); *d = 0;
    return (okm >> i) & 1 ? TXT_WHITE : TXT_GREY;
}
static void of(UINT8 i, char *l1, char *l2){
    char *d; UINT16 c;
    scat(l1, "FD LF OR CR HN");
    if(!has_tech(10)){ scat(l2, "NEEDS STAR CHART"); return; }
    if(!((okm >> i) & 1)){ scat(l2, "NOT VISITED YET"); return; }
    c = 20 * (HV->outp[i] + 1);
    d = scat(l2, "A: LEVEL "); d = u16s(d, HV->outp[i] + 1); d = scat(d, " $"); d = u16s(d, c); d = scat(d, " B2");
}
void hu2_outposts(void) BANKED {
    UINT8 r;
    page_open(); lp_sel = 0; okm = hs_kmask();
    for(;;){
        r = listpage("OUTPOSTS", 12, ol, of, 0);
        if(r >= 253) break;
        if(!has_tech(10) || !((okm >> r) & 1) || HV->outp[r] >= 3){ sfx(SFX_HURT); continue; }
        { UINT16 c = 20 * (HV->outp[r] + 1);
          if(HV->shells >= c && HV->res[R_BAR] >= 2){ HV->shells -= c; HV->res[R_BAR] -= 2; HV->outp[r]++; sfx(SFX_OK); } else sfx(SFX_HURT); }
    }
}
/* ---------------------------------------------------------------- link to the game */
static UINT8 kl(UINT8 i, char *o){
    static const char *const t[6] = { "GIVE 200 GOLD", "GIVE ORE HELD", "GIVE CRYSTAL HELD", "GIVE FOOD HELD", "CLAIM TRIBUTE", "ROYAL VISIT" };
    scat(o, t[i]);
    if(i == 4 && HV->claim == HV->day + 1) return TXT_GREY;
    if(i == 5 && (HV->royal || !sv.friends)) return TXT_GREY;
    return TXT_WHITE;
}
static UINT16 tribute(void){ UINT16 g = 20 + 3 * (UINT16)HV->pop + 5 * (UINT16)HV->rwon + 10 * (UINT16)HV->boss; return g > 400 ? 400 : g; }
static void kf(UINT8 i, char *l1, char *l2){
    char *d;
    switch(i){
        case 0: scat(l1, "200 GOLD=10 SHELLS"); scat(l2, "FROM YOUR PURSE"); break;
        case 1: d = scat(l1, "SHIP HOLD: "); d = u16s(d, sv.el.cargo[G_ORE]); scat(l2, "EACH UNIT = 3 ORE"); break;
        case 2: d = scat(l1, "SHIP HOLD: "); d = u16s(d, sv.el.cargo[G_CRYSTAL]); scat(l2, "EACH UNIT = 2 CRYS"); break;
        case 3: d = scat(l1, "FOOD+NECTAR: "); d = u16s(d, sv.el.cargo[G_FOOD] + sv.el.cargo[G_NECTAR]); scat(l2, "FOOD X3, HONEY X2"); break;
        case 4: d = scat(l1, "A SMALL GIFT: "); d = u16s(d, tribute()); scat(l2, "GOLD, ONCE A DAY"); break;
        default: scat(l1, sv.friends ? "A FRIEND VISITS" : "NEEDS A FRIEND"); scat(l2, "+2 HOMES, HAPPIER"); break;
    }
}
void hu2_link(void) BANKED {
    UINT8 r, n;
    page_open(); lp_sel = 0;
    for(;;){
        r = listpage("LINK TO THE GAME", 6, kl, kf, 0);
        if(r >= 253) break;
        switch(r){
            case 0: if(sv.gold >= 200){ sv.gold -= 200; HV->shells += 10; sfx(SFX_COIN); } else sfx(SFX_HURT); break;
            case 1: n = sv.el.cargo[G_ORE]; if(n){ sv.el.cargo[G_ORE] = 0; HV->res[R_ORE] += 3 * (UINT16)n; if(HV->res[R_ORE] > 999) HV->res[R_ORE] = 999; sfx(SFX_PICK); } else sfx(SFX_HURT); break;
            case 2: n = sv.el.cargo[G_CRYSTAL]; if(n){ sv.el.cargo[G_CRYSTAL] = 0; HV->res[R_CRYS] += 2 * (UINT16)n; if(HV->res[R_CRYS] > 999) HV->res[R_CRYS] = 999; sfx(SFX_PICK); } else sfx(SFX_HURT); break;
            case 3: { UINT16 f = sv.el.cargo[G_FOOD], h = sv.el.cargo[G_NECTAR];
                      if(f + h){ sv.el.cargo[G_FOOD] = 0; sv.el.cargo[G_NECTAR] = 0; HV->res[R_FOOD] += 3 * f; HV->res[R_HONEY] += 2 * h; if(HV->res[R_FOOD] > 999) HV->res[R_FOOD] = 999; if(HV->res[R_HONEY] > 999) HV->res[R_HONEY] = 999; sfx(SFX_PICK); } else sfx(SFX_HURT); } break;
            case 4: if(HV->claim != HV->day + 1){
                        UINT16 g = tribute(); HV->claim = HV->day + 1; HV->links++; sv.gold += g;
                        if(!(HV->links & 3) && sv.potions < 9) sv.potions++;
                        sfx(SFX_COIN); slot_write(cur_slot); } else sfx(SFX_HURT); break;
            default: if(sv.friends && !HV->royal){ HV->royal = 1; if(HV->hap < 90) HV->hap += 10; hs_note(M_ROYAL); sfx(SFX_FANFARE); } else sfx(SFX_HURT); break;
        }
    }
}
/* ---------------------------------------------------------------- score and achievements */
static UINT8 al(UINT8 i, char *o){
    o[0] = ((HV->ach >> i) & 1) ? T_CHK_ON : T_CHK_OFF; o[1] = ' '; o[2] = 0; scat(o + 2, aname[i]);
    return ((HV->ach >> i) & 1) ? TXT_GOLD : TXT_GREY;
}
static void af(UINT8 i, char *l1, char *l2){ (void)i; (void)l1; (void)l2; }
void hu2_status(void) BANKED {
    UINT8 redraw = 1, pg = 0, i, a = 0; char b[24], *d; UINT32 sc;
    page_open();
    for(;;){
        if(redraw){
            if(pg == 1){ lp_sel = 0; txt_soft(); txt_soft_end(); { UINT8 r = listpage("ACHIEVEMENTS", 16, al, af, 0); (void)r; } pg = 0; redraw = 1; continue; }
            for(i = 0; i < 16; i++) a += (HV->ach >> i) & 1;
            sc = hs_score();
            txt_soft(); frame_page("COLONY");
            d = scat(b, "SCORE "); d = u32s(d, sc); txt_put(2, 3, b);
            d = scat(b, "BEST  "); d = u32s(d, HV->best > sc ? HV->best : sc); txt_put(2, 4, b);
            d = scat(b, ""); d = scat(d, kind_name[HV->kind]); d = scat(d, " "); d = scat(d, dname[HV->diff]); txt_put(2, 5, b);
            d = scat(b, "DAY "); d = u16s(d, HV->day + 1); d = scat(d, "  BUGS "); d = u16s(d, HV->pop); txt_put(2, 6, b);
            d = scat(b, "WON "); d = u16s(d, HV->rwon); d = scat(d, " LOST "); d = u16s(d, HV->rlost); txt_put(2, 7, b);
            d = scat(b, "BOSS "); d = u16s(d, HV->boss); d = scat(d, " QUEST "); d = u16s(d, HV->qn); txt_put(2, 8, b);
            d = scat(b, "GEAR "); d = u16s(d, HV->gear); d = scat(d, " DEAD "); d = u16s(d, HV->dead); txt_put(2, 9, b);
            d = scat(b, "ACHIEVED "); d = u16s(d, a); d = scat(d, " OF 16"); txt_put(2, 10, b);
            d = scat(b, "NEXT RAID DAY "); d = u16s(d, HV->rday + 1); txt_put(2, 11, b);
            d = scat(b, "FOES "); d = u16s(d, hs_strength(RK_BANDIT)); d = scat(d, " DEF "); d = u16s(d, hs_defence()); txt_put(2, 12, b);
            txt_put(2, 15, "A: ACHIEVEMENTS"); txt_color(2, 15, 16, 1, TXT_GREY);
            txt_soft_end(); redraw = 0; a = 0;
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_B)){ sfx(SFX_BACK); break; }
        if(HIT(J_A)){ pg = 1; redraw = 1; }
    }
}
/* ---------------------------------------------------------------- options */
static UINT8 ol2(UINT8 i, char *o){
    if(i == 0){ char *d = scat(o, "DIFFICULTY "); scat(d, dname[HV->diff]); }
    else if(i == 1) scat(o, HV->stat[S_SPEED] ? "TIME  FAST X3" : "TIME  NORMAL");
    else scat(o, "NEW COLONY");
    return i == 2 ? TXT_RED : TXT_WHITE;
}
static void of2(UINT8 i, char *l1, char *l2){
    if(i == 0){ scat(l1, "RAIDS AND HUNGER"); scat(l2, "LEFT RIGHT: CHANGE"); }
    else if(i == 1){ scat(l1, "SELECT ON THE MAP"); scat(l2, "SWITCHES TIME TOO"); }
    else { scat(l1, "ERASES THE COLONY!"); scat(l2, "PRESS A TWICE"); }
}
UINT8 hu2_options(void) BANKED {
    UINT8 r, sure = 0;
    page_open(); lp_sel = 0;
    for(;;){
        r = listpage("OPTIONS", 3, ol2, of2, 0);
        if(r == 255 || r == 253) break;
        if(r == 254){
            if(lp_sel == 0){ if(lp_dir == 1) HV->diff = HV->diff < 2 ? HV->diff + 1 : 0; else HV->diff = HV->diff ? HV->diff - 1 : 2; }
            else if(lp_sel == 1) HV->stat[S_SPEED] ^= 1;
            continue;
        }
        if(r == 1) HV->stat[S_SPEED] ^= 1;
        if(r == 2){ if(sure){ return 1; } sure = 1; sfx(SFX_HURT); }
    }
    return 0;
}
