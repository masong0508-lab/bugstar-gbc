#pragma bank 26
/* EXTRAS, the small extras that fit the cartridge: MINING (a timing game with a pick), the station LOUNGE (rumours, a drink), RELICS (12 trophies
   that unlock by themselves) and the DEBUG menu (unlocked with SELECT+A on its card). Saved after the kart block, SRAM offset 432 of the slot. */
#include <gb/gb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "spell.h"
#include "kart.h"
#include "warship.h"
#include "hive.h"
#include "xtra.h"
#include "wild.h"

typedef struct { UINT32 seed; UINT16 relics; UINT16 ore; UINT8 best, dbg, lounge, chk; } XSave;
#define XS_OFF 432U
static XSave xs;
static UINT16 xr = 0xBEEF;
static UINT8 xrnd(void){ xr ^= xr << 7; xr ^= xr >> 9; xr ^= xr << 8; return (UINT8)xr; }
static UINT8 xs_sum(void){ const UINT8 *p = (const UINT8 *)&xs; UINT8 i, s = 0x3C; for(i = 0; i < sizeof xs - 1; i++) s ^= (UINT8)(p[i] + i * 3); return s; }
static void xs_load(void){
    UINT8 *p = (UINT8 *)&xs, i;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof xs; i++) p[i] = *(volatile UINT8 *)(0xA000U + XS_OFF + i);
    DISABLE_RAM;
    if(xs.seed != sv.seed || xs.chk != xs_sum()){ memset(&xs, 0, sizeof xs); xs.seed = sv.seed; }
    xr ^= (UINT16)frames32() | 1;
}
static void xs_save(void){
    UINT8 *p = (UINT8 *)&xs, i;
    xs.seed = sv.seed; xs.chk = xs_sum();
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof xs; i++) *(volatile UINT8 *)(0xA000U + XS_OFF + i) = p[i];
    DISABLE_RAM;
    if(opt.autosave) slot_write(cur_slot);
}
static void screen(const char *title){
    txt_win(0); txt_soft(); txt_box(0, 0, 20, 18);
    txt_line(2, 1, title, 16); txt_color(2, 1, 16, 1, TXT_GOLD);
    txt_pal_reset(); SHOW_BKG; DISPLAY_ON;
}
static void gold_line(void){ char b[21], *d = scat(b, "GOLD "); d = u16s(d, sv.gold > 65000UL ? 65000U : (UINT16)sv.gold); *d = 0; txt_line(2, 16, b, 16); txt_color(2, 16, 16, 1, TXT_GREY); }
static void wait_a(void){ for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_B) || HIT(J_START)) return; } }

UINT16 xtra_ore(void) BANKED { xs_load(); return xs.ore; }          /* ores mined in all (the bounty office counts them) */
/* ---------------------------------------------------------------- RELICS */
#define NREL 12
static const char *const rel_nm[NREL] = { "FIRST BLOOD", "CENTURION", "ANGLER", "HELL WALKER", "MOON WALKER", "HELL KING", "MOON KING", "SPEEDSTER", "CUP HOLDER", "GOLD CUP", "SPELLBOUND", "MINER" };
static const char *const rel_rq[NREL] = { "KILL A MONSTER", "100 KILLS", "CATCH 10 FISH", "HELL DEEP FLOOR 5", "MOON DEEP FLOOR 5", "HELL DEEP FLOOR 20", "MOON DEEP FLOOR 15", "KART LAP UNDER 0:40", "ANY KART CUP", "WIN A GOLD CUP", "CAST 50 SPELLS", "MINE 30 ORES" };
static UINT8 rel_ok(UINT8 i){
    switch(i){
    case 0: return EX_KILLS >= 1;   case 1: return EX_KILLS >= 100;  case 2: return EX_FISHN >= 10;
    case 3: return EX_HELL >= 5;    case 4: return EX_MOON >= 5;     case 5: return EX_HELL >= 20;
    case 6: return EX_MOON >= 15;   case 7: return EX_KART && EX_KART < 2400;
    case 8: return ks.cup >= 1;     case 9: return ks.cup >= 3;      case 10: return EX_CASTS >= 50;
    default: return xs.ore >= 30;
    }
}
UINT8 xtra_relics(void) BANKED {                                 /* unlock what is earned (60 gold each) and count them */
    UINT8 i, n = 0, ch = 0;
    xs_load();
    for(i = 0; i < NREL; i++){
        if(!(xs.relics & (1U << i)) && rel_ok(i)){ xs.relics |= 1U << i; sv.gold += 60; ch = 1; }
        if(xs.relics & (1U << i)) n++;
    }
    if(ch) xs_save();
    return n;
}
void xtra_relic_screen(void) BANKED {
    UINT8 i, top = 0, n; char b[21], *d;
    n = xtra_relics();
    for(;;){
        screen("RELICS");
        d = u16s(b, n); d = scat(d, " OF 12"); *d = 0; txt_line(12, 1, b, 7); txt_color(12, 1, 7, 1, TXT_WHITE);
        for(i = 0; i < 5; i++){
            UINT8 k = top + i, y = 3 + i * 2, got = (xs.relics >> k) & 1;
            txt_line(2, y, got ? "*" : "-", 1); txt_color(2, y, 1, 1, got ? TXT_GOLD : TXT_GREY);
            txt_line(4, y, rel_nm[k], 14); txt_color(4, y, 14, 1, got ? TXT_GOLD : TXT_GREY);
            txt_line(4, y + 1, rel_rq[k], 14); txt_color(4, y + 1, 14, 1, TXT_GREY);
        }
        txt_line(18, 3, top ? "^" : " ", 1); txt_line(18, 12, top + 5 < NREL ? "v" : " ", 1);
        txt_line(2, 15, "UP/DOWN  B: BACK", 16); txt_color(2, 15, 16, 1, TXT_GREY);
        for(;;){ wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START) || HIT(J_A)) return;
            if(HITR(J_DOWN) && top + 5 < NREL){ top += 5; if(top + 5 > NREL) top = NREL - 5; sfx(SFX_MOVE); break; }
            if(HITR(J_UP) && top){ top = top > 5 ? top - 5 : 0; sfx(SFX_MOVE); break; } }
    }
}

/* ---------------------------------------------------------------- MINING: press A when the pick is over the ore */
/* Pure scoring, shared by the real timing game and the host regression.
   A successful hit is measured from the marker the player actually sees. */
static UINT8 mine_rank(UINT8 marker, UINT8 target){
    UINT8 center=target+1, dist=marker>center?marker-center:center-marker;
    return dist==0?4:dist==1?3:dist==2?2:dist<=4?1:0;
}
static UINT16 mine_swing_pay(UINT8 rank, UINT8 *streak, UINT8 *best){
    static const UINT8 val[5]={0,4,10,20,40};
    UINT16 pay=val[rank];
    if(rank>=3){
        if(++*streak>1) pay+=(UINT16)*streak*3;
        if(*streak>*best) *best=*streak;
    }else *streak=0;
    return pay;
}
static UINT16 mine_total_pay(UINT16 pay,UINT8 completed,UINT8 best,UINT8 bonus){
    if(completed==8 && best>=4) pay+=100;
    return pay+(UINT16)((UINT32)pay*bonus/100u);
}
void xtra_mine(void) BANKED {
    UINT8 sw,m,dir,t,tick,hit,ok,prev,got=0,streak=0,best_st=0,completed=0,stop=0;
    UINT16 pay=0; char b[21],bar[19],*d;
    static const char *const nm[5]={"A ROCK. NOTHING.","COPPER! +4","SILVER! +10","GOLD! +20","A GEM! +40"};
    xs_load();
    screen("MINING");
    txt_line(2,3,"PRESS A WHEN THE",16);txt_line(2,4,"PICK IS OVER ORE",16);txt_color(2,3,16,2,TXT_GREY);
    txt_line(2,12,"8 SWINGS FOR BONUS",16);
    txt_line(2,14,"A:START  B:BACK",16);txt_color(2,14,16,1,TXT_GREY);
    for(;;){wait_vbl_done();input_poll();if(HIT(J_B)||HIT(J_START))return;if(HIT(J_A))break;}
    for(sw=0;sw<8 && !stop;sw++){
        t=3+xrnd()%10;m=0;dir=1;tick=0;hit=0;
        d=scat(b,"SWING ");d=u16s(d,sw+1);d=scat(d," OF 8");*d=0;txt_line(2,3,b,16);txt_color(2,3,16,1,TXT_WHITE);
        txt_line(2,4,"A: SWING",16);txt_line(2,10,"",16);
        txt_line(2,14,"B: CASH OUT",16);txt_color(2,14,16,1,TXT_GREY);
        /* Paint the target band once. A moving marker only changes two
           tile cells; stationary frames don't touch text or palette VRAM.
           The 18-cell bar fits inside the 20-column panel. */
        for(ok=0;ok<18;ok++)bar[ok]=ok>=t && ok<=t+2?'O':'.';
        bar[0]='V';bar[18]=0;txt_line(1,8,bar,18);
        txt_color(1,8,18,1,TXT_WHITE);txt_color(1+t,8,3,1,TXT_GOLD);
        while(!hit){
            wait_vbl_done();input_poll();
            if(HIT(J_B)||HIT(J_START)){hit=2;break;}
            if(HIT(J_A)){hit=1;break;} /* score the displayed marker BEFORE advancing it */
            if(++tick>=(UINT8)(5-(sw>>1))){
                tick=0;prev=m;
                if(dir){if(++m>=17)dir=0;}else if(m==0||--m==0)dir=1;
                if(prev!=m){
                    txt_line(1+prev,8,prev>=t && prev<=t+2?"O":".",1);
                    txt_line(1+m,8,"V",1);
                }
            }
        }
        if(hit==2)break;
        ok=mine_rank(m,t);pay+=mine_swing_pay(ok,&streak,&best_st);
        completed++;if(ok)got++;
        txt_line(2,10,nm[ok],16);txt_color(2,10,16,1,ok?TXT_GOLD:TXT_RED);
        d=scat(b,"EARNED ");d=u16s(d,pay);d=scat(d,"G");*d=0;txt_line(2,12,b,16);
        sfx(ok?SFX_COIN:SFX_HURT);
        for(tick=0;tick<40;tick++){
            wait_vbl_done();input_poll();
            if(HIT(J_B)||HIT(J_START)){stop=1;break;}
            if(HIT(J_A))break; /* skip the result hold, not another swing */
        }
    }
    if(!completed)return;
    screen("MINING");
    d=scat(b,"ORES ");d=u16s(d,got);*d=0;txt_line(4,5,b,14);
    if(completed==8 && best_st>=4){txt_line(2,11,"MOTHERLODE! +100",16);txt_color(2,11,16,1,TXT_GOLD);}
    else if(completed<8){txt_line(2,11,"CASHED OUT EARLY",16);txt_color(2,11,16,1,TXT_GREY);}
    pay=mine_total_pay(pay,completed,best_st,weird_pct(weird_of(sv.star),WP_ORE));
    d=scat(b,"PAY +");d=u16s(d,pay);d=scat(d," G");*d=0;txt_line(4,7,b,14);txt_color(4,7,14,1,TXT_GOLD);
    /* Keep the existing modulo-16-bit mining counter: the bounty office
       deliberately computes its progress across wrap. Only actual hits
       are counted and the session has exactly one payment point. */
    sv.gold+=pay;xs.ore+=got;if(pay>xs.best)xs.best=pay>255?255:(UINT8)pay;
    d=scat(b,"BEST ");d=u16s(d,xs.best);*d=0;txt_line(4,9,b,14);txt_color(4,9,14,1,TXT_GREY);
    sfx(pay>=100?SFX_FANFARE:SFX_OK);xs_save();gold_line();
    txt_line(2,14,"A: OK",8);wait_a();
}

/* ---------------------------------------------------------------- the station LOUNGE */
static const char *const tips[12] = {
    "SELECT CASTS YOUR\nSPELL. A+SELECT\nSWAPS SPELLS.", "B+SELECT DRINKS A\nPOTION WHEN YOU\nARE HURT.",
    "GRAB A BOOK: NEW\nSPELLS COME AT\nLEVELS 1 3 5 8.", "THE QUEEN CAN NOT\nBE KILLED BY A\nSPELL. BITE HER!",
    "SOME FLOORS ARE\nMAZES. FIND THE\nSTAIRS BY TORCH.", "FEATS PAY GOLD:\nCHECK THE FEATS\nPAGE IN SYSTEM.",
    "HELL DEEP GETS\nHARDER EVERY 4\nFLOORS. CASH OUT.", "MOON DEEP IS COOLER\nBUT PAYS MORE.\nTIER EVERY 5.",
    "KART: DRIFT LONG\nBENDS FOR A TURBO\nBOOST.", "KART: HIT A JUMP\nPAD AND PRESS B\nFOR A TRICK.",
    "FISH IN THE PONDS\nBRINGS FEATS AND\nGOLD.", "RELICS UNLOCK BY\nTHEMSELVES. EACH\nPAYS 60 GOLD." };
static void tip_show(UINT8 i){
    const char *s = tips[i % 12]; UINT8 y = 8; char b[21]; UINT8 k;
    for(;;){ for(k = 0; k < 20 && s[k] && s[k] != '\n'; k++) b[k] = s[k]; b[k] = 0; txt_line(2, y, b, 18); txt_color(2, y, 18, 1, TXT_WHITE); y++; s += k; if(!*s) break; s++; }
}
/* THE BARMAN'S RIDDLE: three answers, one is right; 25 gold for it */
static void riddle(void){
    static const char *const q[8] = { "WHAT HAS KEYS BUT\nOPENS NO DOOR?", "WHAT GETS WETTER\nAS IT DRIES?", "WHAT RUNS BUT HAS\nNO LEGS?", "WHAT HAS A HEAD\nAND NO BODY?", "WHAT BREAKS WHEN\nYOU SAY IT?", "WHAT GOES UP BUT\nNEVER COMES DOWN?", "WHAT HAS HANDS BUT\nCAN NOT CLAP?", "WHAT HAS EYES BUT\nCAN NOT SEE?" };
    static const char *const a[8][3] = { {"A PIANO","A CASTLE","A MAP"}, {"A TOWEL","A SPONGE","A CLOUD"}, {"A BEETLE","A RIVER","A SHADOW"}, {"A COIN","A TREE","A WORM"}, {"AN EGG","SILENCE","GLASS"}, {"A KITE","AGE","SMOKE"}, {"A CLOCK","A GLOVE","A CAT"}, {"A POTATO","A BAT","A MOON"} };
    static const UINT8 ans[8] = { 0, 0, 1, 0, 1, 1, 0, 0 };
    UINT8 i = (UINT8)(xrnd() & 7), sel = 0, row, d = 1, k;
    screen("RIDDLE"); { const char *t = q[i]; UINT8 y = 3; char b[21]; for(;;){ for(k = 0; k < 20 && t[k] && t[k] != '\n'; k++) b[k] = t[k]; b[k] = 0; txt_line(2, y, b, 18); y++; t += k; if(!*t) break; t++; } }
    for(;;){
        if(d){ for(row = 0; row < 3; row++){ txt_line(2, 7 + row, row == sel ? S_CURSOR : " ", 1); txt_line(4, 7 + row, a[i][row], 14); txt_color(2, 7 + row, 18, 1, row == sel ? TXT_GOLD : TXT_WHITE); } d = 0; }
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) return;
        if(HITR(J_UP)){ sel = sel ? sel - 1 : 2; d = 1; sfx(SFX_MOVE); }
        if(HITR(J_DOWN)){ sel = sel < 2 ? sel + 1 : 0; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_A)) break;
    }
    if(sel == ans[i]){ sv.gold += 25; txt_line(2, 11, "RIGHT! +25 GOLD", 18); txt_color(2, 11, 18, 1, TXT_GOLD); sfx(SFX_FANFARE); gold_line(); }
    else { txt_line(2, 11, "WRONG. HA!", 18); txt_color(2, 11, 18, 1, TXT_RED); sfx(SFX_HURT); }
    txt_line(2, 14, "A: OK", 8); wait_a();
}
void xtra_lounge(void) BANKED {
    UINT8 sel = 0, d = 1, tip = 0, row;
    xs_load(); tip = xs.lounge;
    for(;;){
        if(d){
            screen("STATION LOUNGE");
            txt_line(2, 3, "THE BARMAN NODS.", 18); txt_color(2, 3, 18, 1, TXT_GREY);
            for(row = 0; row < 4; row++){ txt_line(2, 5 + row, row == sel ? S_CURSOR : " ", 1); txt_line(4, 5 + row, row == 0 ? "LISTEN TO A RUMOUR" : row == 1 ? "BUY A DRINK 10G" : row == 2 ? "A RIDDLE: 25G" : "LEAVE", 16); txt_color(2, 5 + row, 18, 1, row == sel ? TXT_GOLD : TXT_WHITE); }
            gold_line(); d = 0;
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) break;
        if(HITR(J_UP)){ sel = sel ? sel - 1 : 3; d = 1; sfx(SFX_MOVE); }
        if(HITR(J_DOWN)){ sel = sel < 3 ? sel + 1 : 0; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_A)){
            if(sel == 3) break;
            if(sel == 2){ riddle(); d = 1; xs_save(); continue; }
            if(sel == 0){ d = 1; screen("A RUMOUR"); tip_show(tip); tip = (tip + 1) % 12; xs.lounge = tip; sfx(SFX_OK); txt_line(2, 14, "A: OK", 8); wait_a(); }
            else if(sv.gold < 10){ txt_line(2, 10, "NOT ENOUGH GOLD.", 18); txt_color(2, 10, 18, 1, TXT_RED); sfx(SFX_HURT); }
            else if(sv.hp >= hud_hpmax(sv.level)){ txt_line(2, 10, "YOU ARE FINE.", 18); txt_color(2, 10, 18, 1, TXT_GREY); }
            else { sv.gold -= 10; sv.hp = hud_hpmax(sv.level); gold_line(); txt_line(2, 10, "AHH. FULL HEARTS!", 18); txt_color(2, 10, 18, 1, TXT_GOLD); sfx(SFX_LEVEL); }
        }
    }
    xs_save();
}

/* ---------------------------------------------------------------- DEBUG */
UINT8 xtra_dbg_open(UINT8 unlock) BANKED {
    xs_load();
    if(unlock && !xs.dbg){ xs.dbg = 1; xs_save(); }
    return xs.dbg;
}
void xtra_debug(void) BANKED {
    UINT8 sel = 0, d = 1, row; char b[21], *e;
    static const char *const it[6] = { "+1000 GOLD", "LEVEL UP", "FULL HEARTS", "UNLOCK ALL RELICS", "CLEAR KART TIMES", "LEAVE" };
    xs_load();
    for(;;){
        if(d){
            screen("DEBUG");
            for(row = 0; row < 6; row++){ txt_line(2, 3 + row, row == sel ? S_CURSOR : " ", 1); txt_line(4, 3 + row, it[row], 16); txt_color(2, 3 + row, 18, 1, row == sel ? TXT_GOLD : TXT_WHITE); }
            e = scat(b, "LEVEL "); e = u16s(e, sv.level); *e = 0; txt_line(2, 12, b, 16); txt_color(2, 12, 16, 1, TXT_GREY);
            gold_line(); d = 0;
        }
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)) break;
        if(HITR(J_UP)){ sel = sel ? sel - 1 : 5; d = 1; sfx(SFX_MOVE); }
        if(HITR(J_DOWN)){ sel = sel < 5 ? sel + 1 : 0; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_A)){
            d = 1; sfx(SFX_OK);
            if(sel == 0) sv.gold += 1000;
            else if(sel == 1){ if(sv.level < 999) sv.level++; sv.hp = hud_hpmax(sv.level); }
            else if(sel == 2) sv.hp = hud_hpmax(sv.level);
            else if(sel == 3) xs.relics = 0x0FFF;
            else if(sel == 4){ memset(ks.tt, 0, sizeof ks.tt); ks.cup = 0; EX_KART = 0; kart_ks_save(); kart_rec_reset(); warship_reset(); hive_reset(); }
            else break;
        }
    }
    xs_save();
}
