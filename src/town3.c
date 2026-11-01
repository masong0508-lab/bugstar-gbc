#pragma bank 40
/* WILD, part 5: TOWN EXTRAS 2. Four more places for the towns (they come after town2.c's, same window look):
   the MARKET (sell and buy the eight goods at the town's prices), the BOUNTY OFFICE (a job paid the moment it is done: kills, fish, pats,
   friends, ore, pirates, floors, races), the CANTINA (soup, gossip with true tips, a round for fame, arm wrestling) and the RACE POST
   (a quick kart race for an entry fee). The job in hand, the rumours heard, the race wins live in the WILD block (wild.h XA). */
#include <gb/gb.h>
#include <string.h>
#include "wram.h"
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "bug.h"
#include "econ.h"
#include "feat.h"
#include "spell.h"
#include "xtra.h"
#include "kart.h"
#include "dialog.h"
#include "wild.h"

static const char *const nm[4] = { "BOUNTY OFFICE", "CANTINA", "RACE POST", "MARKET" };
void town3_name(UINT8 id, char *out) BANKED { scat(out, nm[id - SV3_BOUNTY]); }
UINT8 town3_open(UINT8 id, UINT8 night) BANKED { return id == SV3_CANTINA || !night; }
UINT8 town3_add(UINT8 *ids, UINT8 n, UINT8 tier, UINT16 th) BANKED {
    (void)th;
    if(tier >= 1){ ids[n++] = SV3_MARKET; ids[n++] = SV3_BOUNTY; ids[n++] = SV3_CANTINA; }
    if(tier >= 2) ids[n++] = SV3_RACE;
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
static void seed_rs(UINT16 th){ rs = (UINT16)frames32() ^ ((UINT16)DIV_REG << 8) ^ th; if(!rs) rs = 1; }

/* ---------------------------------------------------------------- the bounty office */
static const char *const jv[9] = { "", "SQUASH ", "CATCH ", "PAT ", "FREE ", "MINE ", "SHOOT ", "GO ", "WIN " };
static const char *const jo[9] = { "", "MONSTERS", "FISH", "ANIMALS", "BUG FRIENDS", "ORES", "PIRATES", "FLOORS DEEPER", "RACES" };
static UINT16 cnt(UINT8 t, const XA *x){                          /* the counter a job type watches */
    switch(t){
        case 1: return EX_KILLS;   case 2: return EX_FISHN;     case 3: return x->petn;       case 4: return sv.friends;
        case 5: return xtra_ore(); case 6: return sv.el.kills;   case 7: return sv.deepest;    default: return x->race;
    }
}
static UINT16 w16(const UINT8 *p){ return p[0] | ((UINT16)p[1] << 8); }
static UINT8 bj_done(const XA *x){ return x->bt && cnt(x->bt, x) >= w16(x->bq) + x->bn; }
static UINT16 bj_pay(XA *x){                                      /* pays and clears the job in x (the caller saves) */
    UINT16 g = w16(x->bg);
    sv.gold += g; fame_add(4); gain_xp(6 + sv.level); x->bt = 0; if(x->bdone < 255) x->bdone++;
    return g;
}
void bounty_tick(void) BANKED {
    XA x; UINT16 g; char b[40], *d;
    xa_get(&x);
    if(!x.bt || !bj_done(&x)) return;
    g = bj_pay(&x); xa_put(&x); sfx(SFX_FANFARE);
    d = u16s(scat(b, "JOB DONE! THE OFFICE WIRED "), g); scat(d, "G. BEEP."); dlg_say(0, b);
}
static UINT8 bj_ok(UINT8 t, UINT8 tier, const XA *x){            /* can this job be done at all (a counter that stopped at 255 cannot) */
    if(t == 2) return EX_FISHN < 240;
    if(t == 3) return x->petn < 240;
    if(t == 4) return sv.friends < 245;
    if(t == 7) return sv.deepest < 88;
    if(t == 8) return tier >= 2 && x->race < 250;
    return 1;
}
static void bounty(UINT8 tier, UINT16 th){
    XA x; char l1[24], w[20], b[24], *d; const char *m = 0; UINT8 t, n, have, mp = TXT_GREY; UINT16 pay;
    static const UINT8 base[9] = { 0, 5, 3, 3, 1, 4, 3, 2, 1 };
    w[0] = 0;
    for(;;){
        xa_get(&x);
        if(x.bt && bj_done(&x)){ UINT16 g = bj_pay(&x); xa_put(&x); d = u16s(scat(w, "WIRED "), g); scat(d, "G. BEEP."); m = "JOB DONE!"; mp = TXT_GOLD; sfx(SFX_FANFARE); }
        have = x.bt; pay = w16(x.bg);
        if(have){ t = x.bt; n = x.bn; }
        else {                                                      /* the job on offer: the same for this town until you take it */
            UINT16 r = th ^ ((UINT16)x.bdone * 0x3D5u); r ^= r >> 6; r *= 0x2D3Bu; r ^= r >> 5;
            t = 1 + (UINT8)(r % 8);
            while(!bj_ok(t, tier, &x)) t = t % 8 + 1;
            n = base[t] + (t == 4 || t == 7 || t == 8 ? 0 : (UINT8)((r >> 4) & 3)) + (t == 1 ? sv.level / 5 : 0);
            if(t == 7) n = 1 + (UINT8)((r >> 4) & 3);
            pay = 30 + 6 * (UINT16)sv.level + 9 * (UINT16)n + (t == 8 ? 40 : 0) + (t == 7 ? 20 * (UINT16)n : 0); pay = LUCKY(pay);
        }
        d = u16s(scat(l1, jv[t]), n); d = scat(d, " "); scat(d, jo[t]);
        page("BOUNTY OFFICE");
        line(4, have ? "YOUR JOB:" : "THE CLERK OFFERS:", TXT_GREY);
        line(6, l1, TXT_WHITE);
        if(have){ UINT16 so = cnt(t, &x), bs = w16(x.bq); d = u16s(scat(b, "SO FAR "), so > bs ? so - bs : 0); d = u16s(scat(d, "/"), n); *d = 0; line(7, b, TXT_WHITE); }
        d = u16s(scat(b, "PAYS "), pay); scat(d, "G AND FAME"); line(9, b, TXT_GOLD);
        d = u16s(scat(b, "DONE BEFORE: "), x.bdone); *d = 0; line(11, b, TXT_GREY);
        line(13, m ? m : "PAID WHEN DONE", m ? mp : TXT_GREY);
        if(m && w[0]) line(14, w, TXT_GOLD);
        line(16, have ? "SEL: DROP  B: BACK" : "A: TAKE  B: BACK", TXT_GREY);
        m = 0; mp = TXT_GREY; w[0] = 0; l1[0] = 0;
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A) && !have){
                UINT16 c = cnt(t, &x);
                x.bt = t; x.bn = n; x.bq[0] = (UINT8)c; x.bq[1] = (UINT8)(c >> 8); x.bg[0] = (UINT8)pay; x.bg[1] = (UINT8)(pay >> 8); xa_put(&x);
                sfx(SFX_OK); m = "GOOD HUNTING, BUG!"; mp = TXT_GOLD; break;
            }
            if(HIT(J_SELECT) && have){ x.bt = 0; xa_put(&x); sfx(SFX_BACK); m = "JOB DROPPED"; mp = TXT_RED; break; }
        }
    }
}

/* ---------------------------------------------------------------- the cantina */
static const char rum[24][3][19] = {
    { "BEETLES HATE", "SIDESTEPS. JUST", "SAYING." },
    { "A PIRATE DOCKED", "LAST WEEK. HE", "TIPPED WELL." },
    { "THE BANK PAYS 1%", "EVERY 5 MINUTES", "OF PLAY. SAVE UP." },
    { "GOLDEN FISH ARE", "ONE IN SIXTEEN.", "KEEP CASTING." },
    { "THE TAILOR DYES", "YOUR SHELL. SAME", "BUG, NEW COLOURS." },
    { "SELECT CASTS A", "SPELL. B+SELECT", "DRINKS A POTION." },
    { "THE BARTENDER IS", "A BEETLE. ARM", "WRESTLE HIM." },
    { "THERE IS SOUP.", "THERE IS ALWAYS", "SOUP." },
    { "A GOOD ROUND BUYS", "FAME. FAME BUYS", "PERKS." },
    { "WEIRD WORLDS CAN", "CHANGE THE COLOUR", "OF THE SKY." },
    { "A FULL HOLD EARNS", "NOTHING. GO AND", "SELL IT OFF." },
    { "PRICES SWING FROM", "STAR TO STAR.", "BUY LOW." },
    { "THE QUEEN NEVER", "DIES FROM SPELLS.", "BITE HER." },
    { "A LABYRINTH FLOOR", "HIDES THE STAIRS.", "KEEP WALKING." },
    { "A PORTAL SPELL", "SAVES YOUR FLOOR.", "LEVEL 10." },
    { "THE PIT PAYS BY", "THE WAVE. GIVE UP", "WHEN IT HURTS." },
    { "HELL WORLDS HAVE", "NO BOTTOM. THEY", "PAY WELL." },
    { "THE MOON DUNGEON", "IS COOLER. AND", "DEEPER. AND WORSE." },
    { "KART WORLD PAYS", "FOR WINS, NOT", "FOR EFFORT." },
    { "THE SHIP KNOWS", "WHAT TO DO NEXT.", "ASK BOLT." },
    { "I HAVE NEVER LEFT.", "THE VIEW IS THE", "SAME." },
    { "MINING PAYS MORE", "WHEN YOU TIME IT.", "WAIT FOR THE ORE." },
    { "FEATS PAY GOLD.", "OPEN THE SYSTEM", "MENU. LOOK." },
    { "A WARM CANTINA IS", "A FINE PLACE TO", "BE. STAY AWHILE." } };
static void tip(char *l1, char *l2, char *l3){                      /* a true tip: a weird world you have not visited */
    UINT8 s = rnd(), i, w = 0;
    for(i = 0; i < 255; i++, s++){
        if(s == sv.star || (ex.seen[s >> 3] & (1 << (s & 7)))) continue;
        w = weird_of(s); if(w) break;
    }
    if(!w){ scat(l1, "NO NEW WEIRD"); scat(l2, "WORLDS NEAR HERE."); scat(l3, "TRY AGAIN LATER."); return; }
    { char *d = u16s(scat(l1, "WORLD "), s); scat(d, " IS WEIRD:"); }
    weird_name(l2, w); scat(l3, "NOT VISITED YET.");
}
static void gossip(char (*g)[20]){                                   /* one rumour (or a tip) into three lines */
    XA x; UINT8 k, i; char *a = g[0], *b = g[1], *c = g[2];
    a[0] = b[0] = c[0] = 0;
    xa_get(&x);
    if(!(rnd() & 3)) tip(a, b, c);
    else {
        k = rnd() % 24;
        for(i = 0; i < 24; i++, k = (k + 1) % 24) if(!((x.gos >> k) & 1)) break;
        if(i == 24){ x.gos = 0; k = rnd() % 24; }
        x.gos |= 1UL << k; xa_put(&x);
        scat(a, rum[k][0]); scat(b, rum[k][1]); scat(c, rum[k][2]);
    }
}
static void cantina(UINT8 tier, UINT16 th){
    static const char *const it[4] = { "BOWL OF SOUP", "LISTEN FOR GOSSIP", "BUY A ROUND  40G", "ARM WRESTLE  20G" };
    UINT8 sel = 0, i; UINT16 soup = 5 + 2 * (UINT16)tier; char b[24], *d, gl[3][20]; const char *m = 0; UINT8 mp = TXT_GREY;
    seed_rs(th); gl[0][0] = 0;
    for(;;){
        page("CANTINA");
        for(i = 0; i < 4; i++){ d = scat(b, i == sel ? S_CURSOR : " "); d = scat(d, it[i]); if(i == 0) u16s(scat(d, " "), soup); line(10 + i, b, i == sel ? TXT_GOLD : TXT_WHITE); }
        if(m) line(5, m, mp);
        else if(gl[0][0]){ line(5, gl[0], TXT_WHITE); line(6, gl[1], TXT_WHITE); line(7, gl[2], TXT_WHITE); }
        else { line(5, "IT IS WARM IN HERE", TXT_GREY); line(6, "THE BARTENDER IS A", TXT_GREY); line(7, "VERY LARGE BEETLE.", TXT_GREY); }
        line(15, "A: DO IT  B: BACK", TXT_GREY);
        m = 0; gl[0][0] = 0;
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_UP)){ sel = sel ? sel - 1 : 3; sfx(SFX_MOVE); break; }
            if(HIT(J_DOWN)){ sel = (sel + 1) & 3; sfx(SFX_MOVE); break; }
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A)){
                mp = TXT_RED;
                if(sel == 0){
                    if(sv.gold < soup){ m = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
                    else { UINT16 mx = hud_hpmax(sv.level), mm = hud_mpmax(sv.level); sv.gold -= soup; sv.hp += mx / 2; if(sv.hp > mx) sv.hp = mx; sv.mp += mm / 2; if(sv.mp > mm) sv.mp = mm; m = "GOOD SOUP! BETTER."; mp = TXT_GOLD; sfx(SFX_COIN); }
                } else if(sel == 1){ gossip(gl); sfx(SFX_OK); }
                else if(sel == 2){
                    if(sv.gold < 40){ m = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
                    else { sv.gold -= 40; fame_add(2); unlocks_update(); m = "CHEERS! +2 FAME"; mp = TXT_GOLD; sfx(SFX_FANFARE); }
                } else {
                    if(sv.gold < 20){ m = "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
                    else {
                        UINT8 pct = 35 + 4 * UPG(SK_MIGHT); if(pct > 85) pct = 85;
                        if((rnd() % 100) < pct){ sv.gold += 30; gain_xp(2 + sv.level / 2); m = "YOU WIN! +30 GOLD"; mp = TXT_GOLD; sfx(SFX_FANFARE); }
                        else { sv.gold -= 20; m = "BEETLE WINS! -20G"; sfx(SFX_HURT); }
                    }
                }
                break;
            }
        }
    }
}

/* ---------------------------------------------------------------- the race post */
static void racepost(UINT8 tier, UINT16 th){
    XA x; UINT8 trk = (UINT8)(th >> 5) % 3, place; UINT16 fee = 20 + 10 * (UINT16)tier; char b[24], *d; const char *m = 0; UINT8 mp = TXT_GREY;
    for(;;){
        xa_get(&x);
        page("RACE POST");
        line(4, "A QUICK KART RACE.", TXT_GREY); line(5, "3 LAPS. 3 RIVALS.", TXT_GREY);
        d = u16s(scat(b, "TRACK "), trk + 1); line(7, b, TXT_WHITE);
        d = u16s(scat(b, "ENTRY "), fee); scat(d, " GOLD"); line(9, b, TXT_GOLD);
        d = u16s(scat(b, "RACES WON: "), x.race); line(11, b, TXT_GREY);
        line(13, m ? m : "FIRST PLACE PAYS", m ? mp : TXT_GREY);
        line(15, "A: ENTER  B: BACK", TXT_GREY);
        m = 0; mp = TXT_RED;
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A)){
                if(sv.gold < fee){ m = "NOT ENOUGH GOLD"; sfx(SFX_BACK); break; }
                sv.gold -= fee;
                place = kart_post(trk);
                if(place == 1){
                    UINT16 g = LUCKY(60 + 10 * (UINT16)tier);
                    xa_get(&x); if(x.race < 255) x.race++; xa_put(&x);
                    sv.gold += g; fame_add(3); unlocks_update(); mp = TXT_GOLD; m = "YOU WON! PAID!"; sfx(SFX_FANFARE);
                } else if(place){ m = "NOT FIRST. AGAIN?"; }
                else m = "YOU LEFT THE RACE";
                trk = (trk + 1) % 3;
                break;
            }
        }
    }
}

/* ---------------------------------------------------------------- the market */
static void market(UINT8 tier){
    UINT8 sel = 0, i, kind = world_kind(world_seed(sv.star)), left = 0; char b[24], g[12], *d; const char *m = 0; UINT8 mp = TXT_GREY;
    (void)tier;
    for(;;){
        page("MARKET");
        line(4, "SELL OR BUY GOODS.", TXT_GREY);
        for(i = 0; i < G_N; i++){
            good_label(i, g); d = scat(b, i == sel ? S_CURSOR : " "); d = scat(d, g);
            while((UINT8)(d - b) < 11) *d++ = ' ';
            d = scat(d, "x"); d = u16s(d, sv.el.cargo[i]); *d = 0;
            line(5 + i, b, i == sel ? TXT_GOLD : TXT_WHITE);
        }
        d = u16s(scat(b, "SELL "), econ_sell(sv.star, kind, sel)); d = u16s(scat(d, "  BUY "), econ_price(sv.star, kind, sel)); line(14, b, TXT_GOLD);
        d = u16s(scat(b, "HOLD "), econ_load()); d = u16s(scat(d, "/"), HOLD_CAP()); line(15, b, TXT_GREY);
        line(16, m ? m : "A:ALL L:ONE R:BUY", m ? mp : TXT_GREY);
        m = 0; mp = TXT_GREY; left = 0;
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_UP)){ sel = sel ? sel - 1 : G_N - 1; sfx(SFX_MOVE); break; }
            if(HIT(J_DOWN)){ sel = (sel + 1) % G_N; sfx(SFX_MOVE); break; }
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A) || HIT(J_LEFT)){
                left = econ_sell_n(sv.star, kind, sel, HIT(J_A) ? 99 : 1);
                if(left){ m = "SOLD."; mp = TXT_GOLD; sfx(SFX_COIN); } else { m = "NONE TO SELL"; mp = TXT_RED; sfx(SFX_BACK); }
                break;
            }
            if(HIT(J_RIGHT)){
                if(econ_buy(sv.star, kind, sel, 1)){ m = "BOUGHT."; mp = TXT_GOLD; sfx(SFX_COIN); }
                else { m = econ_load() >= HOLD_CAP() ? "YOUR HOLD IS FULL" : "NOT ENOUGH GOLD"; mp = TXT_RED; sfx(SFX_BACK); }
                break;
            }
        }
    }
}

void town3_go(UINT8 id, UINT8 tier, UINT16 th) BANKED {
    seed_rs(th);
    if(id == SV3_BOUNTY) bounty(tier, th);
    else if(id == SV3_CANTINA) cantina(tier, th);
    else if(id == SV3_RACE) racepost(tier, th);
    else market(tier);
}
