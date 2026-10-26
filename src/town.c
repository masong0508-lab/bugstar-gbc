#pragma bank 13
/* BUGSTAR GBC - the cities: a page of places to go (inn, quest board, stores, academy, fortune teller, casino, shrine) over the planet.
   See town.h. The quest in hand lives in sv.el.rsv[2..6] and sv.rsv[0..1] (type, two numbers, the star, how many were done; the reward). */
#include <gb/gb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "menus.h"
#include "econ.h"
#include "shop.h"
#include "town.h"

enum { SV_INN, SV_QUEST, SV_STORE, SV_YARD, SV_ACADEMY, SV_FORTUNE, SV_CASINO, SV_SHRINE };
static const char *const pre[16] = { "BRAM", "WEN", "KOR", "ASH", "MIR", "TOL", "FEN", "DUN", "SAL", "OAK", "VEX", "LUM", "NOR", "PIP", "ZAN", "HOL" };
static const char *const suf[8]  = { "WICK", "TON", "HAVEN", "BURG", "DALE", "FORD", "POLIS", "HOLM" };
static const char *const tier_nm[5] = { "HAMLET", "VILLAGE", "TOWN", "CITY", "METROPOLIS" };
static const char *const hello[8] = { "WELCOME, TRAVELER!", "MIND THE BEETLES.", "FRESH BERRIES!", "THE SHIP IS SAFE.",
                                      "BUGS ARE WELCOME.", "STAY AWHILE!", "GOOD LUCK DOWN THERE", "A QUIET DAY." };
static const char *const pop_s[5] = { "12", "140", "2300", "18000", "400000" };

static UINT16 th, rs;           /* this town's hash, a random state */
static UINT8  shrine_used, mpal;
static char   mb[24];

static UINT16 thash(UINT8 star, UINT8 idx){
    UINT32 ws = world_seed(star); UINT16 h = (UINT16)ws ^ (UINT16)(ws >> 16) ^ ((UINT16)(idx + 1) * 0x9E37u);
    h ^= h >> 7; h *= 0x2D3Bu; h ^= h >> 5; return h;
}
static char *tname(char *d, UINT16 h){ d = scat(d, pre[h & 15]); return scat(d, suf[(h >> 4) & 7]); }
void town_label(char *out, UINT8 star, UINT8 idx, UINT8 tier) BANKED {
    char *d = scat(out, tier_nm[tier]); d = scat(d, " "); tname(d, thash(star, idx));
}
static UINT8 rnd(void){ rs ^= rs << 7; rs ^= rs >> 9; rs ^= rs << 8; return (UINT8)rs; }
static void lv(UINT8 x, UINT8 y, UINT8 w, const char *lab, const char *val, UINT8 vpal){
    char b[24], *d = scat(b, lab); UINT8 n = (UINT8)(d - b);
    d = scat(d, " "); scat(d, val);
    txt_line(x, y, b, w); txt_color(x, y, w, 1, vpal); txt_color(x, y, n, 1, TXT_GREY);
}
static void gold_row(void){ char v[12]; u32s(v, sv.gold > 999999999UL ? 999999999UL : sv.gold); lv(1, 2, 18, "GOLD", v, TXT_GOLD); }
static void page(const char *title){                              /* a whole page: a frame, the title, the gold */
    txt_win(1); txt_cls(); txt_box(0, 0, 20, 18);
    txt_line(1, 1, title, 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    gold_row(); txt_win(0); move_win(7, 0); SHOW_WIN;
}
static void line(UINT8 y, const char *s, UINT8 pal){ txt_win(1); txt_line(1, y, s, 18); txt_color(1, y, 18, 1, pal); txt_win(0); }

/* a page with two lines of words, a price and A to do it: 0 = B (back), 1 = done (the gold is paid), 2 = too poor */
static UINT8 offer(const char *title, const char *l1, const char *l2, UINT16 price, const char *msg){
    char b[24], *d;
    page(title);
    line(5, l1, TXT_WHITE); line(6, l2, TXT_WHITE);
    if(price){ d = u16s(scat(b, "PRICE "), price); scat(d, " GOLD"); line(9, b, TXT_GOLD); }
    line(12, msg ? msg : "", mpal);
    line(15, "A: OK    B: BACK", TXT_GREY);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) return 0;
        if(HIT(J_A)){ if(sv.gold < price){ sfx(SFX_BACK); return 2; } sv.gold -= price; return 1; }
    }
}

/* ---------------------------------------------------------------- the places */
static void inn(UINT8 tier){
    const char *m = 0; UINT8 r; UINT16 c = 4 + tier * 4;
    for(;;){
        r = offer("THE INN", "A SOFT BED AND A", "HOT BREAKFAST.", c, m);
        if(!r) break;
        if(r == 1){ sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level); m = "SLEPT WELL! HEALED."; mpal = TXT_GOLD; sfx(SFX_FANFARE); }
        else { m = "NOT ENOUGH GOLD"; mpal = TXT_RED; }
    }
}
static void academy(void){
    const char *m = 0; UINT8 r; UINT16 c = 20 * (UINT16)sv.level;
    for(;;){
        c = 20 * (UINT16)sv.level;
        r = offer("ACADEMY", "A WHOLE LEVEL OF", "TRAINING IN A DAY.", c, m);
        if(!r) break;
        if(r == 1){ gain_xp(12 * (UINT16)sv.level); m = "YOU FEEL STRONGER!"; mpal = TXT_GOLD; }
        else { m = "NOT ENOUGH GOLD"; mpal = TXT_RED; }
    }
}
static void fortune(INT8 ddx, INT8 ddy){
    const char *m = 0; char l2[20], *d; UINT8 r, ax, ay, s; const char *v, *h;
    ax = (UINT8)(ddx < 0 ? -ddx : ddx); ay = (UINT8)(ddy < 0 ? -ddy : ddy);
    s = ax > ay ? ax + ay / 2 : ay + ax / 2;
    v = ay * 2 > ax ? (ddy < 0 ? "NORTH" : "SOUTH") : ""; h = ax * 2 > ay ? (ddx < 0 ? "WEST" : "EAST") : "";
    for(;;){
        d = scat(l2, "ABOUT "); d = u16s(d, s); scat(d, " STEPS");
        r = offer("FORTUNE TELLER", "I SEE THE DOOR DOWN", l2, 10, m);
        if(!r) break;
        if(r == 1){ mb[0] = 0; d = scat(mb, v); if(*v && *h) d = scat(d, " "); d = scat(d, h); scat(d, " OF HERE"); m = mb; mpal = TXT_GOLD; sfx(SFX_OK); }
        else { m = "NOT ENOUGH GOLD"; mpal = TXT_RED; }
    }
}
static void shrine(UINT8 tier){
    const char *m = 0; UINT8 r;
    for(;;){
        r = offer("SHRINE", "KNEEL AND ASK FOR", shrine_used ? "NOTHING MORE." : "A BLESSING.", 0, m);
        if(!r) break;
        if(shrine_used){ m = "THE SHRINE IS QUIET"; mpal = TXT_GREY; continue; }
        shrine_used = 1; sv.hp = hud_hpmax(sv.level); sv.mp = hud_mpmax(sv.level); sfx(SFX_FANFARE); mpal = TXT_GOLD;
        r = rnd() % 3;
        if(r == 0){ UINT16 g = 25 * (UINT16)(tier + 1); sv.gold += g; mb[0] = 0; scat(u16s(scat(mb, "+"), g), " GOLD"); m = mb; }
        else if(r == 1){ if(sv.potions < 9){ sv.potions++; m = "A HEALTH POTION!"; } else { sv.gold += 30; m = "+30 GOLD"; } }
        else { UINT16 x = 8 + 4 * (UINT16)sv.level; gain_xp(x); mb[0] = 0; scat(u16s(scat(mb, "+"), x), " XP"); m = mb; }
    }
}
static void casino(void){
    static const UINT8 bets[4] = { 10, 25, 50, 100 };
    UINT8 bet = 0, cup = 1, win = 3, shown = 0; const char *m = 0; char b[24], *d; UINT8 i;
    for(;;){
        page("CASINO");
        line(4, "FIND THE GEM IN", TXT_GREY);
        line(5, "3 CUPS. WIN: X2", TXT_GREY);
        d = u16s(scat(b, "BET "), bets[bet]); line(8, b, TXT_GOLD);
        txt_win(1);
        for(i = 0; i < 3; i++){
            char c[6]; d = scat(c, i == cup ? "[" : " "); d = scat(d, shown ? (i == win ? "*" : ".") : "?"); scat(d, i == cup ? "]" : " ");
            txt_put(3 + i * 5, 11, c); txt_color(3 + i * 5, 11, 3, 1, i == cup ? TXT_GOLD : TXT_WHITE);
        }
        txt_win(0);
        line(13, m ? m : "", mpal);
        line(15, "L/R: CUP U/D: BET", TXT_GREY); line(16, "A: PLAY B: LEAVE", TXT_GREY);
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_LEFT)  && cup) { cup--; shown = 0; break; }
            if(HIT(J_RIGHT) && cup < 2) { cup++; shown = 0; break; }
            if(HIT(J_UP))   { bet = (bet + 1) & 3; sfx(SFX_MOVE); break; }
            if(HIT(J_DOWN)) { bet = (bet + 3) & 3; sfx(SFX_MOVE); break; }
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A)){
                if(sv.gold < bets[bet]){ m = "NOT ENOUGH GOLD"; mpal = TXT_RED; sfx(SFX_BACK); break; }
                win = rnd() % 3; shown = 1;
                if(win == cup){ sv.gold += (UINT16)bets[bet] * 2; m = "YOU WIN!"; mpal = TXT_GOLD; sfx(SFX_FANFARE); }
                else { sv.gold -= bets[bet]; m = "NOT THAT ONE..."; mpal = TXT_RED; sfx(SFX_HURT); }
                break;
            }
        }
    }
}

/* ---------------------------------------------------------------- the quest board */
#define Q_T sv.el.rsv[2]
#define Q_A sv.el.rsv[3]
#define Q_B sv.el.rsv[4]
#define Q_S sv.el.rsv[5]
#define Q_N sv.el.rsv[6]
#define Q_R (sv.rsv[0] | ((UINT16)sv.rsv[1] << 8))
static UINT8 qt, qa, qb, qs; static UINT16 qr;
static void q_offer(UINT8 star, UINT8 idx, UINT8 tier){
    UINT16 r = th ^ ((UINT16)Q_N * 0x3D5u); UINT8 t; r ^= r >> 6; r *= 0x2D3Bu; r ^= r >> 5;
    t = 1 + (UINT8)(r % 4); qs = star; qb = 0;
    if(t == 3 && sv.friends >= 250) t = 1;
    qt = t;
    if(t == 1){ qa = (idx + 1 + ((r >> 3) & 1)) % 3; qr = 40 + 25 * (UINT16)tier + (r & 15); }
    else if(t == 2){ qa = sv.deepest < 90 ? sv.deepest + 2 : 92; qr = 50 + 8 * (UINT16)qa + 20 * (UINT16)tier; }
    else if(t == 3){ qa = sv.friends + 1; qr = 90 + 40 * (UINT16)tier; }
    else { qa = (UINT8)((r >> 4) & 7); qb = 2 + tier; qr = econ_base(qa) * (UINT16)qb * 3 / 2 + 25; }
}
static void q_load(void){ qt = Q_T; qa = Q_A; qb = Q_B; qs = Q_S; qr = Q_R; }
static UINT8 q_done(UINT8 idx){
    if(qt == 1) return sv.star == qs && idx == qa;
    if(qt == 2) return sv.deepest >= qa;
    if(qt == 3) return sv.friends >= qa;
    return sv.el.cargo[qa] >= qb;
}
static void q_text(char *l1, char *l2){
    char *d;
    if(qt == 1){ scat(l1, "PARCEL FOR:"); tname(l2, thash(qs, qa)); }
    else if(qt == 2){ scat(l1, "REACH DUNGEON"); d = u16s(scat(l2, "FLOOR "), qa); d = u16s(scat(d, " (BEST "), sv.deepest); scat(d, ")"); }
    else if(qt == 3){ scat(l1, "FREE A BUG FRIEND"); d = u16s(scat(l2, "FROM A CAGE: "), sv.friends); d = u16s(scat(d, "/"), qa); }
    else { char g[12]; good_label(qa, g); d = u16s(scat(l1, "BRING "), qb); d = scat(d, " "); scat(d, g); scat(l2, "FROM YOUR HOLD"); }
}
static void quest(UINT8 star, UINT8 idx, UINT8 tier){
    char l1[22], l2[22], b[24], *d; const char *m = 0; UINT8 have, ok;
    for(;;){
        have = Q_T; if(have) q_load(); else q_offer(star, idx, tier);
        l1[0] = 0; l2[0] = 0; q_text(l1, l2); ok = have && q_done(idx);
        page("QUEST BOARD");
        line(4, have ? "YOUR QUEST:" : "A JOB FOR YOU:", TXT_GREY);
        line(6, l1, TXT_WHITE); line(7, l2, TXT_WHITE);
        d = u16s(scat(b, "REWARD "), qr); scat(d, " GOLD"); line(10, b, TXT_GOLD);
        line(12, m ? m : (ok ? "IT IS DONE!" : have ? (qt == 1 ? "FIND THAT TOWN" : "NOT DONE YET") : ""), ok ? TXT_GOLD : TXT_GREY);
        line(15, have ? (ok ? "A: CLAIM  B: BACK" : "SEL: DROP  B: BACK") : "A: TAKE  B: BACK", TXT_GREY);
        m = 0;
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A) && !have){ Q_T = qt; Q_A = qa; Q_B = qb; Q_S = qs; sv.rsv[0] = (UINT8)qr; sv.rsv[1] = (UINT8)(qr >> 8); sfx(SFX_OK); m = "GOOD LUCK, BUG!"; break; }
            if(HIT(J_A) && ok){
                UINT16 g = LUCKY(qr);
                if(qt == 4) sv.el.cargo[qa] -= qb;
                sv.gold += g; fame_add(3 + tier * 2); unlocks_update(); gain_xp(5 + (qr > 400 ? 40 : qr / 10));
                Q_T = 0; Q_N++; sfx(SFX_FANFARE); m = "THANK YOU! PAID."; break;
            }
            if(HIT(J_SELECT) && have){ Q_T = 0; sfx(SFX_BACK); m = "QUEST DROPPED"; break; }
        }
    }
}

/* ---------------------------------------------------------------- the town page */
static const char *const sv_nm[8] = { "THE INN", "QUEST BOARD", "GENERAL STORE", "SHIPYARD", "ACADEMY", "FORTUNE TELLER", "CASINO", "SHRINE" };
void town_run(UINT8 star, UINT8 idx, UINT8 tier, INT8 ddx, INT8 ddy) BANKED {
    UINT8 ids[8], n = 0, sel = 0, i, sprites = LCDC_REG & LCDCF_OBJON, redraw = 1, sp; char b[24], *d;
    th = thash(star, idx); shrine_used = 0; mpal = TXT_GOLD;
    rs = (UINT16)frames32() ^ ((UINT16)DIV_REG << 8) ^ th; if(!rs) rs = 1;
    ids[n++] = SV_INN; ids[n++] = SV_QUEST;
    if(tier >= 1) ids[n++] = SV_STORE;
    if(tier >= 3) ids[n++] = SV_YARD;
    sp = 0;                                                         /* the special places: 0, 1, 2 or all four of them */
    { UINT8 k, cnt = tier < 2 ? 0 : tier == 2 ? 1 : tier == 3 ? 2 : 4, s0 = (UINT8)(th >> 11) & 3;
      for(k = 0; k < cnt; k++) sp |= 1 << ((s0 + k) & 3);
      for(k = 0; k < 4; k++) if(sp & (1 << k)) ids[n++] = SV_ACADEMY + k; }
    HIDE_SPRITES; sfx(SFX_OK);
    for(;;){
        if(redraw){
            page("");
            d = tname(b, th); line(1, b, TXT_GOLD);
            d = scat(b, tier_nm[tier]); d = scat(d, "  POP "); scat(d, pop_s[tier]); line(3, b, TXT_GREY);
            line(4, hello[(th >> 8) & 7], TXT_GREY);
            redraw = 0;
        }
        txt_win(1);
        for(i = 0; i < n; i++){
            d = scat(b, i == sel ? S_CURSOR : " "); scat(d, sv_nm[ids[i]]); txt_line(1, 6 + i, b, 18);
            txt_color(1, 6 + i, 18, 1, i == sel ? TXT_GOLD : TXT_WHITE);
        }
        txt_line(1, 16, "A: GO IN  B: LEAVE", 18); txt_color(1, 16, 18, 1, TXT_GREY);
        txt_win(0);
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : n - 1; sfx(SFX_MOVE); }
        if(HIT(J_DOWN)) { sel = sel + 1 < n ? sel + 1 : 0; sfx(SFX_MOVE); }
        if(HIT(J_B) || HIT(J_START)) break;
        if(HIT(J_A)){
            UINT8 id = ids[sel]; mpal = TXT_GOLD; sfx(SFX_OK);
            if(id == SV_INN) inn(tier);
            else if(id == SV_QUEST) quest(star, idx, tier);
            else if(id == SV_STORE){ shop_run(0); }
            else if(id == SV_YARD){ shop_run(1); }
            else if(id == SV_ACADEMY) academy();
            else if(id == SV_FORTUNE) fortune(ddx, ddy);
            else if(id == SV_CASINO) casino();
            else shrine(tier);
            redraw = 1;
        }
    }
    sfx(SFX_BACK);
    txt_win(1); txt_cls(); txt_win(0);
    hud_redraw(); hud_pos(HUD_Y);
    if(sprites) SHOW_SPRITES;
}
