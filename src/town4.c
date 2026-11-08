#pragma bank 52
/* BUGSTAR GBC - WILD, part 6: TOWNSFOLK. Every town has three people (their names and trades come from the town's hash). Talk to them for the news; each one
   will ask a favour of you when you have none in hand, so there is always one ERRAND (kept in the WILD block, xr[8..19]):
   POTIONS (hand over some health potions), ORE (from the ship's hold), FISH (from your bag), SLAY (beat that many monsters in dungeons, counted from now),
   LETTER (carry it to a world you have already visited and hand it in at any town there). The pay is gold, XP and a little fame; the errand is turned in at
   the TOWNSFOLK place of any town (the letter at one on its world). SELECT drops the errand. */
#include <gb/gb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "econ.h"
#include "spell.h"
#include "xtra.h"
#include "wild.h"

#define E_TYPE xr[8]     /* 0 none, 1 potions, 2 ore, 3 letter, 4 slay, 5 fish */
#define E_N    xr[9]     /* how many (the target star for a letter) */
#define E_BASE 10        /* xr[10..11]: the kill counter when it was taken */
#define E_PAY  12        /* xr[12..13]: the pay */
#define E_DONE xr[14]    /* errands done */
#define E_WHO  xr[15]    /* who asked (name index) */
#define E_JOB  xr[16]    /* and their trade */

static const char *const nm_s[16] = { "ABBY", "BORIS", "CLEO", "DUKE", "EMMA", "FINN", "GRETA", "HANK", "IRIS", "JOSE", "KIRA", "LARS", "MIMI", "NED", "OTTO", "PIA" };
static const char *const job_s[12] = { "BAKER", "SMITH", "FISHER", "GUARD", "MINER", "TEACHER", "FARMER", "SAILOR", "TAILOR", "MONK", "CLERK", "KID" };
static const char chat[12][3][19] = {
    { "BERRY ROLLS ARE", "HOT AT SUNRISE.", "COME EARLY!" },
    { "A SHARP FANG WINS", "HALF THE FIGHT.", "SO THEY SAY." },
    { "THE BIG ONES BITE", "AFTER THE RAIN.", "BRING A NET." },
    { "BEETLES NEAR THE", "GATE LAST NIGHT.", "I SAW THEM." },
    { "ORE SINGS WHEN YOU", "KNOCK ON IT.", "LISTEN CLOSELY." },
    { "STARS ARE SUNS.", "EACH WITH A WORLD.", "ISN'T THAT NICE?" },
    { "THE BERRIES ARE", "EARLY THIS YEAR.", "A GOOD SIGN." },
    { "THE ROUTES ARE SAFE", "UNTIL THEY AREN'T.", "FLY CAREFULLY." },
    { "A NEW COAT OF DYE", "MAKES A NEW BUG.", "COME BY LATER." },
    { "SLEEP AT THE INN.", "HEAL AT THE SHRINE.", "BOTH ARE KIND." },
    { "FORMS, FORMS,", "FORMS. NOBODY", "READS THEM." },
    { "I SAW A SPIDER!", "IT WAS HUGE. ALMOST", "AS BIG AS MY HAND." }
};
static const char mood[8][19] = { "NICE DAY, ISN'T IT?", "BUSY TODAY.", "YOU AGAIN! WELCOME.", "HAVE YOU EATEN?", "THE TOWN IS QUIET.", "NEWS TRAVELS SLOW.", "MIND THE BEETLES.", "GOOD LUCK OUT THERE." };

static void line(UINT8 y, const char *s, UINT8 pal){ txt_win(1); txt_line(1, y, s, 18); txt_color(1, y, 18, 1, pal); txt_win(0); }
static void page(const char *title){
    txt_win(1); txt_soft(); txt_box(0, 0, 20, 18);
    txt_line(1, 1, title, 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    txt_win(0); txt_soft_end(); move_win(7, 0); SHOW_WIN;
}
static UINT16 w16(const UINT8 *p){ return p[0] | ((UINT16)p[1] << 8); }
static UINT16 rs = 1;
static UINT8 rnd(void){ rs ^= rs << 7; rs ^= rs >> 9; rs ^= rs << 8; return (UINT8)rs; }

UINT8 town4_add(UINT8 *ids, UINT8 n) BANKED { ids[n++] = SV4_FOLK; return n; }
void town4_name(UINT8 id, char *out) BANKED { (void)id; scat(out, "TOWNSFOLK"); }
UINT8 town4_open(UINT8 id, UINT8 night) BANKED { (void)id; (void)night; return 1; }

static UINT8 other_seen(UINT8 s){ return s != sv.star && (ex.seen[s >> 3] & (1 << (s & 7))); }
static UINT8 seen_count(void){ UINT16 s; UINT8 n = 0; for(s = 0; s < 256; s++) if(other_seen((UINT8)s)) n++; return n; }     /* the other worlds you have visited */
static UINT8 seen_nth(UINT8 k){ UINT16 s; for(s = 0; s < 256; s++) if(other_seen((UINT8)s)){ if(!k) return (UINT8)s; k--; } return 0; }
static UINT8 e_have(const XA *x, UINT8 t){                         /* what you hold for this errand, as a number to compare with the need */
    switch(t){
        case 1: return sv.potions;
        case 2: return sv.el.cargo[G_ORE];
        case 5: return ex.fish;
        case 4: { UINT16 k = EX_KILLS, b = w16(&x->xr[E_BASE]); return k > b ? (k - b > 255 ? 255 : (UINT8)(k - b)) : 0; }
        default: return sv.star == x->E_N ? 1 : 0;
    }
}
static void e_text(const XA *x, char *l1, char *l2){
    char *d; UINT8 t = x->E_TYPE, n = x->E_N;
    l2[0] = 0;
    switch(t){
        case 1: d = u16s(scat(l1, "GIVE "), n); scat(d, " POTIONS"); break;
        case 2: d = u16s(scat(l1, "GIVE "), n); scat(d, " ORE (HOLD)"); break;
        case 5: d = u16s(scat(l1, "GIVE "), n); scat(d, " FISH"); break;
        case 4: d = u16s(scat(l1, "SLAY "), n); scat(d, " MONSTERS"); break;
        default: scat(l1, "A LETTER FOR"); d = scat(l2, "WORLD "); world_name(d, world_seed(n)); break;
    }
}
static UINT8 e_done(const XA *x){ UINT8 t = x->E_TYPE; return t && e_have(x, t) >= (t == 3 ? 1 : x->E_N); }
static void e_pay(XA *x){                                          /* hand over what was asked and pay */
    UINT8 t = x->E_TYPE, n = x->E_N; UINT16 g = w16(&x->xr[E_PAY]);
    if(t == 1) sv.potions -= n; else if(t == 2) sv.el.cargo[G_ORE] -= n; else if(t == 5) ex.fish -= n;
    sv.gold += g; fame_add(2); gain_xp(5 + sv.level / 2);
    x->E_TYPE = 0; if(x->E_DONE < 255) x->E_DONE++;
}
static void new_errand(XA *x, UINT8 who, UINT8 job, UINT8 tier){
    UINT8 t = 1 + job % 5, n, k = 0; UINT16 pay, lv = sv.level > 200 ? 200 : sv.level;
    if(t == 3){ k = seen_count(); if(!k) t = 4; }
    switch(t){
        case 1: n = 1 + (rnd() & 1) + tier / 2; pay = 70 * (UINT16)n + lv; break;
        case 2: n = 3 + (rnd() & 3); pay = 45 * (UINT16)n + lv; break;
        case 5: n = 2 + (rnd() & 3); pay = 35 * (UINT16)n + lv; break;
        case 4: n = 4 + tier * 2 + (rnd() & 3); pay = 12 * (UINT16)n + 2 * lv; break;
        default: {
            UINT8 s = seen_nth(rnd() % k); INT8 dx = (INT8)((s & 15) - (sv.star & 15)), dy = (INT8)((s >> 4) - (sv.star >> 4));
            n = s; pay = 60 + 12 * (UINT16)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy)) + 3 * lv; break; }
    }
    x->E_TYPE = t; x->E_N = n; x->xr[E_PAY] = (UINT8)pay; x->xr[E_PAY + 1] = (UINT8)(pay >> 8); x->E_WHO = who; x->E_JOB = job;
    { UINT16 kc = EX_KILLS; x->xr[E_BASE] = (UINT8)kc; x->xr[E_BASE + 1] = (UINT8)(kc >> 8); }
}
void town4_go(UINT8 id, UINT8 tier, UINT16 th) BANKED {
    XA x; UINT8 sel = 0, rows, who[3], job[3], i, have, redraw = 1, shown = 0; const char *m = 0; UINT8 mp = TXT_GREY; char b[24], l2[24], *d;
    (void)id;
    rs = (UINT16)frames32() ^ ((UINT16)DIV_REG << 8) ^ th; if(!rs) rs = 1;
    for(i = 0; i < 3; i++){ who[i] = (UINT8)((th >> (3 * i)) ^ (th >> 9) ^ (i * 5)) & 15; job[i] = (UINT8)(((th >> (2 + 3 * i)) + i * 3 + (th >> 11)) % 12); }
    for(;;){
        xa_get(&x); have = x.E_TYPE != 0; rows = have ? 4 : 3;
        if(redraw){
            page("TOWNSFOLK");
            for(i = 0; i < rows; i++){
                d = scat(b, i == sel ? S_CURSOR : " ");
                if(have && i == 0){ d = scat(d, "YOUR ERRAND"); if(e_done(&x)) scat(d, " *"); }
                else { UINT8 p = have ? i - 1 : i; d = scat(d, nm_s[who[p]]); d = scat(d, ", "); scat(d, job_s[job[p]]); }
                line(3 + i, b, i == sel ? TXT_GOLD : TXT_WHITE);
            }
            redraw = 0;
        }
        for(i = 0; i < 9; i++) line(8 + i, "", TXT_GREY);
        if(shown){
            if(have && sel == 0){                                  /* the errand: what, who, how far you are, the pay */
                e_text(&x, b, l2); line(8, b, TXT_WHITE); line(9, l2, TXT_WHITE);
                d = scat(b, "FROM "); d = scat(d, nm_s[x.E_WHO & 15]); d = scat(d, ", "); scat(d, job_s[x.E_JOB % 12]); line(10, b, TXT_GREY);
                if(x.E_TYPE != 3){ d = u16s(scat(b, "YOU HAVE "), e_have(&x, x.E_TYPE)); d = u16s(scat(d, " / "), x.E_N); line(11, b, TXT_GREY); }
                d = u16s(scat(b, "PAYS "), w16(&x.xr[E_PAY])); scat(d, "G + FAME"); line(12, b, TXT_GOLD);
                line(14, e_done(&x) ? "A: TURN IT IN" : "SEL: DROP IT", e_done(&x) ? TXT_GOLD : TXT_GREY);
            } else {
                UINT8 p = have ? sel - 1 : sel;
                for(i = 0; i < 3; i++) line(8 + i, chat[job[p]][i], TXT_WHITE);
                line(11, mood[(UINT8)(rs + p) & 7], TXT_GREY);
                line(13, have ? "BUSY WITH YOUR ERRAND" : "A: ASK FOR WORK", have ? TXT_GREY : TXT_GOLD);
            }
        }
        if(m){ line(15, m, mp); m = 0; }
        line(16, "B: BACK", TXT_GREY);
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HITR(J_UP)){ sel = sel ? sel - 1 : rows - 1; shown = 1; redraw = 1; sfx(SFX_MOVE); break; }
            if(HITR(J_DOWN)){ sel = sel + 1 < rows ? sel + 1 : 0; shown = 1; redraw = 1; sfx(SFX_MOVE); break; }
            if(HIT(J_SELECT) && have && sel == 0){ x.E_TYPE = 0; xa_put(&x); sfx(SFX_BACK); m = "ERRAND DROPPED"; mp = TXT_RED; redraw = 1; sel = 0; break; }
            if(HIT(J_A)){
                shown = 1;
                if(have && sel == 0){
                    if(e_done(&x)){ UINT16 g = w16(&x.xr[E_PAY]); e_pay(&x); xa_put(&x); d = u16s(scat(b, "PAID "), g); scat(d, "G. THANKS!"); m = b; mp = TXT_GOLD; sfx(SFX_QUEST); redraw = 1; sel = 0; line(15, m, mp); m = 0; }
                    else sfx(SFX_BACK);
                } else if(!have){
                    new_errand(&x, who[sel], job[sel], tier); xa_put(&x); m = "GOOD. COME BACK SOON!"; mp = TXT_GOLD; sfx(SFX_OK); sel = 0; redraw = 1;
                } else sfx(SFX_BACK);
                break;
            }
        }
    }
}
