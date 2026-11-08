#pragma bank 48
/* BUGSTAR GBC - BUG HIVE, the pages and all the texts (hive_int.h). Every page is the window layer over the whole screen, drawn with the shadow-buffer text
   engine (txt_soft): a page that draws the same thing again writes nothing. */
#include <gb/gb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "econ.h"
#include "hive_int.h"

extern volatile UINT8 snd_song;
static const char *const mtext[M_N] = {
    "", "BUILT!", "DUG OUT", "NEED MORE LEAF", "NOT ENOUGH STORES", "NEEDS RESEARCH", "BLOCKED", "FULLY UPGRADED", "CANNOT DO THAT", "TOO MANY SITES", "BUILD A STORE",
    "RAIN BEGINS", "A STORM!", "SNOW FALLS", "A DRY SPELL", "THE SKY CLEARS", "DROUGHT: LESS FOOD", "PLAGUE IN THE HIVE", "TRADERS ARE HERE", "FLOWERS BLOOM!", "RAIDERS!", "RAID DRIVEN OFF!",
    "THE HIVE WAS LOOTED", "AN EGG HATCHED", "A BUG STARVED", "NEEDS DEEP DIG", "SPRING COMES", "SUMMER COMES", "AUTUMN COMES", "WINTER COMES", "RESEARCH DONE!", "QUEST PAID!",
    "NEW ACHIEVEMENT!", "A NEW DAY", "NIGHT FALLS", "TIME X3", "NORMAL TIME", "BOSS RAID!", "A BUG LEFT SADLY", "MARKED TO DIG", "MARK REMOVED", "WOULD TRAP THE HIVE",
    "UPGRADED!", "ALREADY RESEARCHING", "NOTHING TO DO", "PLACE GUARDS", "ROYAL VISIT!", "RAIDERS TOMORROW!", "A:DIG/BUILD B:MENU" };
void hu_text(UINT8 id, char *out) BANKED { out[0] = 0; if(id < M_N) scat(out, mtext[id]); }
static const char *const bnames[C_N] = { "SOIL", "TUNNEL", "ROCK", "IRON ORE", "CRYSTAL", "SKY", "GRASS", "FLOWERS", "ENTRANCE", "WATER", "BUILDING SITE", "QUEEN ROOM",
    "NURSERY", "FUNGUS FARM", "STOREHOUSE", "BARRACKS", "WORKSHOP", "SMELTER", "WALL", "GATE", "WATCHTOWER", "SHRINE", "MARKET" };
static const char *const bdesc[C_N - C_FIRSTB + 1][2] = {
    { "HATCHES NEW BUGS", "NURSES HELP" }, { "LEAF BECOMES FOOD", "IDLE BUGS HELP" }, { "HOLDS 80 MORE OF", "EACH GOOD A LEVEL" },
    { "HOUSES 2 GUARDS", "A LEVEL" }, { "BARS BECOME GEAR", "FASTER BUILDING," }, { "ORE + LEAF = BARS", "" },
    { "BLOCKS A ROAD", "RAIDERS BREAK IT" }, { "BUGS PASS THROUGH", "RAIDERS BREAK IT" }, { "SHOOTS RAIDERS", "IN RANGE" },
    { "HAPPIER BUGS", "FASTER RESEARCH" }, { "TRADE WITH TOWNS", "" }, { "THE QUEEN'S ROOM", "KEEP HER SAFE" } };
static const char *const rname[R_N] = { "FOOD", "LEAF", "ORE", "BARS", "CRYSTAL", "HONEY" };
static const char *const jname[5] = { "FORAGERS", "BUILDERS", "NURSES", "GUARDS", "MINERS" };
static const char *const jwhat[5] = { "FIND FOOD AND LEAF", "BUILD AND DIG", "TEND THE EGGS", "DEFEND THE HIVE", "DIG ORE, CRYSTAL" };
static const char *const tname[NTECH] = { "TRAILS", "COMPOST", "SMELTING", "MARKETS", "SPEARS", "MASONRY", "ARCHERY", "ALARM", "DEEP DIG", "BREEDING", "STAR CHART", "ASCENSION" };
static const char *const tdesc[NTECH][2] = { { "FORAGERS BRING", "25% MORE." }, { "FARMS MAKE", "50% MORE FOOD." }, { "UNLOCKS SMELTER", "MORE BARS." }, { "UNLOCKS MARKETS", "" },
    { "GUARDS HIT HARDER", "" }, { "UNLOCKS GATES", "" }, { "UNLOCKS TOWERS", "" }, { "ONE MORE GUARD IN", "A DEFEND PHASE" }, { "LETS YOU DIG", "CRYSTAL" }, { "EGGS HATCH 50%", "FASTER." },
    { "OUTPOSTS ON OTHER", "KINDS OF WORLD." }, { "QUEST PAY X2.", "THE QUEEN'S GLORY." } };
static const char *const sname[4] = { "SPRING", "SUMMER", "AUTUMN", "WINTER" };
static const char *const wname[W_N] = { "CLR", "RAI", "STM", "SNW", "DRY" };
static const char *const rkname[3] = { "BANDITS", "RIVAL ANTS", "BOSS BEETLE" };

static void pad(char *d, UINT8 n){ UINT8 l = 0; while(d[l]) l++; while(l < n) d[l++] = ' '; d[l] = 0; }
static UINT8 has_tech(UINT8 t){ return (HV->tech >> t) & 1; }
void hu_bname(UINT8 type, char *out) BANKED { out[0] = 0; scat(out, type < C_N ? bnames[type] : "?"); }
static void page_open(void){ txt_win(1); WX_REG = 7; WY_REG = 0; SHOW_WIN; txt_pal_reset(); }
static void frame_page(const char *title){
    txt_box(0, 0, 20, 18); txt_line(2, 1, title, 16); txt_color(2, 1, 16, 1, TXT_GOLD);
}
/* the clock */
static void clock_text(char *d){
    UINT8 h = (UINT8)((HV->tod * 6 / 5 + 6) % 24);
    d = scat(d, "D"); d = u16s(d, HV->day + 1); d = scat(d, " "); *d = sname[(HV->day / 6) & 3][0]; d[1] = sname[(HV->day / 6) & 3][1]; d[2] = sname[(HV->day / 6) & 3][2]; d[3] = 0; d += 3;
    d = scat(d, " "); if(h < 10) *d++ = '0'; d = u16s(d, h); d = scat(d, "H ");
    d = scat(d, wname[HV->wx]); d = scat(d, " " S_HEART); u16s(d, HV->hap);
}
void hu_cellinfo(UINT8 x, UINT8 y, char *out) BANKED {
    UINT8 c = MAP[CI(x, y)], t = CT(c); char *d = out;
    *d = 0;
    if(t >= C_QUEEN && t != C_SITE){ d = scat(d, bnames[t]); if(t != C_QUEEN){ d = scat(d, " L"); d = u16s(d, CLV(c) + 1); } return; }
    d = scat(d, bnames[t]);
    if(t == C_SOIL || t == C_ORE || t == C_CRYS){ d = scat(d, (c & MARKD) ? " (TO DIG)" : " (A:DIG)"); }
    else if(t == C_TUNNEL) scat(d, " (A:BUILD)");
}
void hu_hud(UINT8 sel) BANKED {
    char b[24], *d; UINT8 x = hm_cx(), y = hm_cy(), c;
    (void)sel;
    if(HV->notet){ b[0] = 0; hu_text(HV->note, b); txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_GOLD); }
    else {
        c = CT(MAP[CI(x, y)]);
        if(c != C_SKY && c != C_GRASS && c != C_FLOWER && c != C_TUNNEL){ hu_cellinfo(x, y, b); txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_WHITE); }
        else { b[0] = 0; clock_text(b); txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_GOLD); }
    }
    d = scat(b, "F"); d = u16s(d, HV->res[R_FOOD]); while(d - b < 5) *d++ = ' ';
    d = scat(d, "L"); d = u16s(d, HV->res[R_LEAF]); while(d - b < 10) *d++ = ' ';
    d = scat(d, "O"); d = u16s(d, HV->res[R_ORE]); while(d - b < 15) *d++ = ' ';
    d = u16s(d, HV->pop); d = scat(d, "/"); d = u16s(d, hs_popcap()); *d = 0; txt_line(0, 1, b, 20); txt_color(0, 1, 20, 1, TXT_WHITE);
    d = scat(b, "B"); d = u16s(d, HV->res[R_BAR]); while(d - b < 5) *d++ = ' ';
    d = scat(d, "C"); d = u16s(d, HV->res[R_CRYS]); while(d - b < 10) *d++ = ' ';
    d = scat(d, "H"); d = u16s(d, HV->res[R_HONEY]); while(d - b < 15) *d++ = ' ';
    d = scat(d, S_COIN); d = u16s(d, HV->shells); *d = 0; txt_line(0, 2, b, 20); txt_color(0, 2, 20, 1, TXT_WHITE);
}
/* ---------------------------------------------------------------- a list page */
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
static void msg_foot(char *l2, UINT8 id){ l2[0] = 0; hu_text(id, l2); }
/* ---------------------------------------------------------------- build */
static UINT8 bx, by;
static const char *const bshort[NBLD] = { "NURSERY", "FUNGUS", "STORE", "BARRACKS", "WORKSHOP", "SMELTER", "WALL", "GATE", "TOWER", "SHRINE", "MARKET" };
static UINT8 bl(UINT8 i, char *o){
    UINT8 t = C_FIRSTB + i, k; UINT16 c[4]; char *d = o;
    d = scat(d, bshort[i]); while(d - o < 9) *d++ = ' ';
    if(!hs_unlocked(t)){ scat(d, "LOCKED"); return TXT_GREY; }
    hs_cost(t, 1, c);
    d = scat(d, "L"); d = u16s(d, c[0]);
    if(c[1]){ d = scat(d, " O"); d = u16s(d, c[1]); }
    k = 0; (void)k; *d = 0;
    return (HV->res[R_LEAF] >= c[0] && HV->res[R_ORE] >= c[1] && HV->res[R_BAR] >= c[2] && HV->res[R_CRYS] >= c[3]) ? TXT_WHITE : TXT_GREY;
}
static void bf(UINT8 i, char *l1, char *l2){
    UINT16 c[4]; char *d = l1; UINT8 t = C_FIRSTB + i;
    hs_cost(t, 1, c);
    if(c[2]){ d = scat(d, "BARS "); d = u16s(d, c[2]); d = scat(d, " "); }
    if(c[3]){ d = scat(d, "CRYSTAL "); d = u16s(d, c[3]); }
    scat(l2, bdesc[i][0]);
}
void hu_build_menu(UINT8 x, UINT8 y) BANKED {
    UINT8 r; bx = x; by = y; page_open(); lp_sel = 0;
    for(;;){
        r = listpage("BUILD HERE", NBLD, bl, bf, 0);
        if(r == 255 || r >= 253) break;
        { UINT8 e = hs_place(bx, by, C_FIRSTB + r);
          if(e){ sfx(SFX_HURT); }
          else { sfx(SFX_OK); hs_note(M_BUILT); break; } }
    }
}
/* ---------------------------------------------------------------- a building */
void hu_cell_menu(UINT8 x, UINT8 y) BANKED {
    UINT8 c = MAP[CI(x, y)], t = CT(c), f = 0, redraw = 1, msg = 0, l; UINT16 co[4]; char b[24], *d;
    page_open();
    for(;;){
        c = MAP[CI(x, y)]; t = CT(c); l = CLV(c) + 1;
        if(redraw){
            txt_soft(); frame_page(bnames[t]);
            if(t == C_SITE){ txt_put(2, 4, "BEING BUILT"); txt_put(2, 6, "BUILDERS WORK"); }
            else if(t == C_QUEEN){
                d = scat(b, "BUGS "); d = u16s(d, HV->pop); d = scat(d, " OF "); d = u16s(d, hs_popcap()); txt_put(2, 4, b);
                d = scat(b, "HAPPY "); d = u16s(d, HV->hap); txt_put(2, 5, b);
                d = scat(b, "EGGS "); d = u16s(d, HV->egg); d = scat(d, "/16"); txt_put(2, 6, b);
                d = scat(b, "GEAR "); d = u16s(d, HV->gear); txt_put(2, 7, b);
                txt_put(2, 9, bdesc[NBLD][0]); txt_put(2, 10, bdesc[NBLD][1]);
            } else {
                d = scat(b, "LEVEL "); d = u16s(d, l); d = scat(d, " OF 8"); txt_put(2, 4, b);
                txt_put(2, 6, bdesc[t - C_FIRSTB][0]); txt_put(2, 7, bdesc[t - C_FIRSTB][1]);
                if(l < 8){
                    hs_cost(t, l + 1, co);
                    txt_put(2, 9, "UPGRADE COSTS:");
                    d = scat(b, "LEAF "); d = u16s(d, co[0]); d = scat(d, "  ORE "); d = u16s(d, co[1]); txt_put(2, 10, b);
                    d = b; *d = 0; if(co[2]){ d = scat(d, "BARS "); d = u16s(d, co[2]); d = scat(d, "  "); } if(co[3]){ d = scat(d, "CRYSTAL "); d = u16s(d, co[3]); } txt_put(2, 11, b);
                    txt_put(2, 14, "A:UPGRADE B:BACK");
                } else txt_put(2, 9, "FULLY UPGRADED");
            }
            if(msg){ b[0] = 0; hu_text(msg, b); txt_line(2, 13, b, 16); txt_color(2, 13, 16, 1, TXT_GOLD); }
            txt_soft_end(); redraw = 0;
        }
        wait_vbl_done(); input_poll(); f++;
        if(HIT(J_B)){ sfx(SFX_BACK); break; }
        if(HIT(J_A) && t >= C_FIRSTB){ msg = hs_upgrade(x, y); if(msg){ sfx(SFX_HURT); } else { msg = M_UPGR; sfx(SFX_OK); } redraw = 1; }
    }
}
/* ---------------------------------------------------------------- jobs */
static void jobs_page(void){
    UINT8 sel = 0, redraw = 1, i, cnt[6]; char b[24], *d;
    page_open();
    for(;;){
        if(redraw){
            for(i = 0; i < 6; i++) cnt[i] = 0;
            for(i = 0; i < HV->pop; i++) cnt[HV->wk[i].job]++;
            txt_soft(); frame_page("JOBS");
            for(i = 0; i < 5; i++){
                d = b; *d++ = i == sel ? T_CURSOR : ' '; d = scat(d, jname[i]); while(d - b < 10) *d++ = ' ';
                d = scat(d, "< "); d = u16s(d, HV->quota[i]); d = scat(d, " >"); while(d - b < 16) *d++ = ' '; d = u16s(d, cnt[(i == 0 ? J_FORAGE : i == 1 ? J_BUILD : i == 2 ? J_NURSE : i == 3 ? J_GUARD : J_MINE)]); *d = 0;
                txt_line(1, 3 + i, b, 18); txt_color(1, 3 + i, 18, 1, i == sel ? TXT_GOLD : TXT_WHITE);
            }
            d = scat(b, "IDLE "); d = u16s(d, cnt[J_IDLE]); d = scat(d, " OF "); d = u16s(d, HV->pop); d = scat(d, " BUGS"); txt_line(2, 9, b, 17);
            txt_line(1, 11, jwhat[sel], 18); txt_color(1, 11, 18, 1, TXT_GREY);
            txt_line(1, 12, "IDLE BUGS WORK THE", 18); txt_line(1, 13, "FARMS AND SMELTERS", 18); txt_color(1, 12, 18, 2, TXT_GREY);
            if(sel == 3){ d = scat(b, "BARRACKS HOLD "); d = u16s(d, hs_guardcap()); txt_line(1, 14, b, 18); }
            else if(sel == 2 && !HV->bcnt[C_NURS]) txt_line(1, 14, "NEEDS A NURSERY", 18); else txt_line(1, 14, "", 18);
            txt_line(1, 16, "LEFT RIGHT: CHANGE", 18); txt_color(1, 16, 18, 1, TXT_GREY);
            txt_soft_end(); redraw = 0;
        }
        wait_vbl_done(); input_poll();
        if(HITR(J_UP)){ sel = sel ? sel - 1 : 4; redraw = 1; sfx(SFX_MOVE); }
        if(HITR(J_DOWN)){ sel = sel < 4 ? sel + 1 : 0; redraw = 1; sfx(SFX_MOVE); }
        if(HITR(J_RIGHT) && HV->quota[sel] < 24){ HV->quota[sel]++; hs_assign(); redraw = 1; sfx(SFX_MOVE); }
        if(HITR(J_LEFT) && HV->quota[sel]){ HV->quota[sel]--; hs_assign(); redraw = 1; sfx(SFX_MOVE); }
        if(HIT(J_B) || HIT(J_A)){ sfx(SFX_BACK); break; }
    }
}
/* ---------------------------------------------------------------- research */
static UINT8 tl(UINT8 i, char *o){
    char *d = scat(o, tname[i]); while(d - o < 12) *d++ = ' ';
    if(has_tech(i)){ scat(d, "DONE"); return TXT_GREY; }
    if(HV->rs == i){ d = scat(d, "WORK "); u16s(d, HV->rsleft); return TXT_GOLD; }
    if(!hs_tech_ok(i)){ scat(d, "LOCKED"); return TXT_GREY; }
    scat(d, "OPEN"); return TXT_WHITE;
}
static void tf(UINT8 i, char *l1, char *l2){
    UINT16 c[5]; char *d = l1;
    scat(l2, tdesc[i][0]);
    if(has_tech(i) || !hs_tech_ok(i)){ if(!has_tech(i)) scat(l1, "NEEDS THE ONE BEFORE"); else scat(l1, tdesc[i][1]); return; }
    hs_tech_cost(i, c);
    d = scat(d, "L"); d = u16s(d, c[0]); if(c[1]){ d = scat(d, " O"); d = u16s(d, c[1]); } if(c[2]){ d = scat(d, " B"); d = u16s(d, c[2]); }
    if(c[3]){ d = scat(d, " C"); d = u16s(d, c[3]); } if(c[4]){ d = scat(d, " H"); d = u16s(d, c[4]); }
}
static void research_page(void){
    UINT8 r;
    page_open();
    for(;;){
        r = listpage("RESEARCH", NTECH, tl, tf, 0);
        if(r >= 253) break;
        { UINT8 e = hs_start_research(r); if(e) sfx(SFX_HURT); else { sfx(SFX_OK); hs_note(M_RESEARCH); } }
    }
}
/* ---------------------------------------------------------------- quests */
static void qtext(UINT8 q, char *d){
    UINT16 v = HV->qv[q];
    switch(HV->qt[q]){
        case 0: d = scat(d, "HAVE "); d = u16s(d, v & 0xFFF); d = scat(d, " "); scat(d, rname[v >> 12]); break;
        case 1: d = scat(d, bnames[v >> 8]); d = scat(d, " LV "); u16s(d, v & 255); break;
        case 2: d = scat(d, "REACH "); d = u16s(d, v); scat(d, " BUGS"); break;
        case 3: d = scat(d, "WIN "); d = u16s(d, v); scat(d, " RAIDS"); break;
        case 4: d = scat(d, "KNOW "); d = u16s(d, v); scat(d, " TECHS"); break;
        case 5: d = scat(d, "VISIT "); d = u16s(d, v); scat(d, " WORLDS"); break;
        case 6: d = scat(d, "LAND ON "); d = scat(d, kind_name[v]); scat(d, " WORLD"); break;
        default: d = scat(d, "REACH DAY "); u16s(d, v); break;
    }
}
static UINT8 ql(UINT8 i, char *o){ qtext(i, o); return hs_quest_done(i) ? TXT_GOLD : TXT_WHITE; }
static void qf(UINT8 i, char *l1, char *l2){
    char *d = l1; UINT16 sh = 8 + 4 * (UINT16)HV->qn;
    if(sh > 60) sh = 60; if(has_tech(11)) sh *= 2;
    if(HV->qt[i] == 5){ d = scat(d, "YOU HAVE BEEN TO "); u16s(d, hs_seen_count()); }
    else if(HV->qt[i] == 6) scat(l1, "FLY THERE");
    d = scat(l2, hs_quest_done(i) ? "A: CLAIM " : "PAYS "); d = u16s(d, sh); scat(d, " SHELLS");
}
static void quest_page(void){
    UINT8 r;
    page_open();
    for(;;){
        r = listpage("QUESTS", NQ, ql, qf, 0);
        if(r == 255) break;
        if(r == 253){ if(HV->shells >= 2){ HV->shells -= 2; hs_quest_new(lp_sel); sfx(SFX_MOVE); } else sfx(SFX_HURT); }
        else if(r < NQ){ if(hs_quest_done(r)){ hs_quest_claim(r); sfx(SFX_COIN); } else sfx(SFX_HURT); }
    }
}
/* ---------------------------------------------------------------- the main menu */
static UINT8 ml(UINT8 i, char *o){
    static const char *const t[9] = { "RESUME", "JOBS", "RESEARCH", "QUESTS", "TRADE", "OUTPOSTS", "LINK TO GAME", "COLONY AND SCORE", "OPTIONS" };
    if(i < 9){ scat(o, t[i]); return TXT_WHITE; }
    scat(o, "SAVE AND LEAVE"); return TXT_GOLD;
}
static void mf(UINT8 i, char *l1, char *l2){
    char *d;
    switch(i){
        case 1: d = scat(l1, "IDLE "); d = u16s(d, hs_nidle()); scat(l2, "SET WHO DOES WHAT"); break;
        case 2: if(HV->rs != 255){ d = scat(l1, "NOW: "); d = scat(d, tname[HV->rs]); d = scat(d, " "); u16s(d, HV->rsleft); } else scat(l1, "NOTHING IN HAND"); break;
        case 3: d = scat(l1, "DONE "); u16s(d, HV->qn); scat(l2, "TIED TO TRAVEL"); break;
        case 4: scat(l1, HV->trader ? "TRADERS ARE HERE!" : "BUY AND SELL"); break;
        case 5: scat(l1, "OTHER WORLDS"); break;
        case 6: scat(l1, "GOLD, ORE, TRIBUTE"); break;
        case 9: scat(l1, "HIVE KEEPS GOING"); scat(l2, "WHILE YOU PLAY"); break;
        default: break;
    }
}
UINT8 hu_main_menu(void) BANKED {
    UINT8 r, out = 0, msel = 0;
    page_open(); lp_sel = 0;
    for(;;){
        r = listpage("BUG HIVE", 10, ml, mf, 0); msel = lp_sel;
        if(r == 255 || r == 253) break;
        if(r == 0) break;
        if(r == 1) jobs_page();
        else if(r == 2) research_page();
        else if(r == 3) quest_page();
        else if(r == 4) hu2_trade();
        else if(r == 5) hu2_outposts();
        else if(r == 6) hu2_link();
        else if(r == 7) hu2_status();
        else if(r == 8){ if(hu2_options()){ HV->stat[S_DEF] = 2; out = 1; break; } }
        else if(r == 9){ out = 1; break; }
        page_open(); lp_sel = msel;
    }
    return out;
}
/* ---------------------------------------------------------------- the first page: choose a world */
static UINT16 imask;
static UINT8 il(UINT8 i, char *o){
    char *d; UINT8 k;
    if(i < 12){
        d = scat(o, kind_name[i]); while(d - o < 8) *d++ = ' ';
        for(k = 0; k < 5; k++) *d++ = hs_prof_at(i, k) >= 3 ? '*' : hs_prof_at(i, k) >= 1 ? '-' : '.';
        *d = 0;
        return (imask >> i) & 1 ? TXT_WHITE : TXT_GREY;
    }
    return TXT_WHITE;
}
static void iff(UINT8 i, char *l1, char *l2){
    scat(l1, "FD LF OR CR HN");
    if(!((imask >> i) & 1)) scat(l2, "NOT VISITED YET");
    else scat(l2, "A: FOUND THE HIVE HERE");
}
UINT8 hu_intro(void) BANKED {
    UINT8 r, k, d = 1;
    page_open();
    imask = hs_kmask(); imask |= 1 << world_kind(world_seed(sv.star));
    txt_soft(); frame_page("A NEW HIVE"); txt_put(2, 4, "A COLONY LIVES"); txt_put(2, 5, "BESIDE THE GAME."); txt_put(2, 6, "IT NEVER GETS IN"); txt_put(2, 7, "YOUR WAY: IGNORE"); txt_put(2, 8, "IT OR RAISE A"); txt_put(2, 9, "HIVE."); txt_put(2, 11, "NEXT: PICK WORLD"); txt_put(2, 15, "A: GO  B: BACK"); txt_soft_end();
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A)) break; if(HIT(J_B)){ return 255; } }
    for(;;){
        lp_sel = world_kind(world_seed(sv.star));
        r = listpage("WHICH WORLD?", 12, il, iff, 0);
        if(r == 255) return 255;
        if(r < 12 && ((imask >> r) & 1)){ k = r; break; }
        sfx(SFX_HURT);
    }
    for(;;){
        txt_soft(); frame_page("DIFFICULTY"); txt_put(2, 4, d == 0 ? ">CALM" : " CALM"); txt_put(2, 5, d == 1 ? ">NORMAL" : " NORMAL"); txt_put(2, 6, d == 2 ? ">HARSH" : " HARSH");
        txt_put(2, 9, "RAIDS, HUNGER AND"); txt_put(2, 10, "SCORE FOLLOW IT."); txt_soft_end();
        wait_vbl_done(); input_poll();
        if(HITR(J_UP)){ d = d ? d - 1 : 2; sfx(SFX_MOVE); }
        if(HITR(J_DOWN)){ d = d < 2 ? d + 1 : 0; sfx(SFX_MOVE); }
        if(HIT(J_A)) break;
        if(HIT(J_B)) return 255;
    }
    return k | (d << 4);
}
/* ---------------------------------------------------------------- raids */
void hu_raid_alert(void) BANKED {
    UINT8 k = HV->rpend - 1, f = 0, sel = 0, redraw = 1; char b[24], *d; UINT16 s, df;
    page_open(); sfx(SFX_HURT);
    s = hs_strength(k); df = hs_defence();
    HV->stat[S_DEF] = 0;
    for(;;){
        if(redraw){
            txt_soft(); frame_page(k == RK_BOSS ? "BOSS RAID!" : "RAIDERS!");
            d = scat(b, rkname[k]); d = scat(d, " COMES"); txt_put(2, 4, b);
            d = scat(b, "THEIR POWER  "); d = u16s(d, s); txt_put(2, 6, b);
            d = scat(b, "YOUR DEFENCE "); d = u16s(d, df); txt_put(2, 7, b);
            txt_put(2, 9, df >= s ? "IT SHOULD HOLD." : "IT WILL FALL!");
            txt_color(2, 9, 18, 1, df >= s ? TXT_WHITE : TXT_RED);
            txt_put(1, 12, sel == 0 ? ">YOU DEFEND IT" : " YOU DEFEND IT"); txt_put(1, 13, sel == 1 ? ">LET GUARDS DEFEND" : " LET GUARDS DEFEND");
            txt_put(2, 15, "PLACE GUARDS AND"); txt_put(2, 16, "WATCH THEM FIGHT.");
            txt_color(2, 15, 16, 2, TXT_GREY);
            txt_soft_end(); redraw = 0;
        }
        wait_vbl_done(); input_poll(); f++;
        if(HITR(J_UP) || HITR(J_DOWN)){ sel ^= 1; redraw = 1; sfx(SFX_MOVE); }
        if(HIT(J_A)){ HV->stat[S_DEF] = sel == 0; break; }
        if(HIT(J_B) || HIT(J_START)){ HV->stat[S_DEF] = 0; break; }
    }
}
void hu_report(UINT8 won) BANKED {
    char b[24], *d;
    page_open();
    txt_soft(); frame_page(won ? "VICTORY!" : "DEFEAT");
    if(won){ txt_put(2, 4, "THE RAIDERS FLEE."); d = scat(b, "LOOT  "); d = u16s(d, 4 + HV->rn); scat(d, " SHELLS"); txt_put(2, 6, b); txt_put(2, 8, "THE BUGS CHEER."); }
    else { txt_put(2, 4, "THEY BROKE IN."); txt_put(2, 6, "STORES LOOTED."); d = scat(b, "BUGS NOW "); u16s(d, HV->pop); txt_put(2, 8, b); txt_put(2, 10, "BUILD WALLS AND"); txt_put(2, 11, "TOWERS, TRY AGAIN"); }
    txt_put(2, 15, "PRESS A"); txt_soft_end();
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B)) break; }
}
