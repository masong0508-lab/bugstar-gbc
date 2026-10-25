#pragma bank 16
/* BUGSTAR GBC - the Elite layer, at a station: docking, the market (8 goods), fuel and repairs, the shipyard (shop.c), the job board
   and the lounge (real trade tips). Numbers are in econ.c. Every screen is a full window over the space scene (sprites hidden). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "menus.h"
#include "dialog.h"
#include "econ.h"
#include "shop.h"
#include "space_int.h"
#include "elite.h"

#define FUEL_PRICE 5
#define KIND(s) star_k[s]

static char *pad(char *d, char *start, UINT8 n){ while((UINT8)(d - start) < n) *d++ = ' '; *d = 0; return d; }
static char *num(char *d, const char *pre, UINT16 v, const char *post){ d = scat(d, pre); d = u16s(d, v); return scat(d, post); }
static char *good_at(char *d, UINT8 g){ good_label(g, d); while(*d) d++; return d; }
static char *rank_at(char *d, UINT8 r){ rank_label(r, d); while(*d) d++; return d; }
static UINT8 here;                                                  /* the star we are docked at */
static UINT8 rep;                                                   /* key repeat counter */

static UINT8 held_hit(UINT8 k){                                     /* a press, then repeats while held */
    if(HIT(k)){ rep = 0; return 1; }
    if(HELD(k)){ if(++rep > 18 && !(rep & 3)) return 1; }
    return 0;
}
static void frame_clear(void){ txt_win(1); txt_cls(); txt_box(0, 0, 20, 18); txt_win(0); move_win(7, 0); SHOW_WIN; }
static UINT16 disc_price(UINT16 p){ UINT16 q = (UINT16)((UINT32)p * (100 - fame_disc()) / 100); return q ? q : 1; }

/* ---------------------------------------------------------------- the market */
static void mk_draw(UINT8 cur, const char *msg){
    UINT8 g; char b[24], *d; UINT8 k = KIND(here);
    txt_win(1);
    d = num(b, "MARKET   HOLD ", econ_load(), "/"); d = u16s(d, HOLD_CAP()); pad(d, b, 18); txt_line(1, 1, b, 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    d = scat(b, "GOLD "); d = u32s(d, sv.gold > 999999999UL ? 999999999UL : sv.gold); pad(d, b, 18); txt_line(1, 2, b, 18);
    txt_line(1, 3, "GOOD    BUY SELL HAVE", 18); txt_color(1, 3, 18, 1, TXT_WHITE);
    for(g = 0; g < G_N; g++){
        UINT16 bp = econ_price(here, k, g), sp = econ_sell(here, k, g), mid = econ_base(g);
        d = scat(b, g == cur ? S_CURSOR : " "); d = good_at(d, g); d = pad(d, b, 8);
        d = u16s(d, bp); d = pad(d, b, 12); d = u16s(d, sp); d = pad(d, b, 17); u16s(d, sv.el.cargo[g]);
        txt_line(1, 4 + g, b, 18);
        txt_color(1, 4 + g, 18, 1, TXT_WHITE);
        if(bp * 100 <= mid * 85) txt_color(9, 4 + g, 3, 1, TXT_GOLD);          /* cheap here: buy! */
        if(sp * 100 >= mid * 112) txt_color(13, 4 + g, 3, 1, TXT_GOLD);        /* dear here: sell! */
    }
    txt_line(1, 13, "R: BUY   L: SELL", 18); txt_color(1, 13, 18, 1, TXT_WHITE);
    txt_line(1, 14, "A: BUY MAX", 18); txt_color(1, 14, 18, 1, TXT_WHITE);
    txt_line(1, 15, "SELECT: SELL ALL", 18); txt_color(1, 15, 18, 1, TXT_WHITE);
    txt_line(1, 16, msg ? msg : "", 18); txt_color(1, 16, 18, 1, TXT_RED);
    txt_win(0);
}
static void market(void){
    UINT8 cur = 0, n; const char *msg = 0;
    frame_clear(); mk_draw(cur, msg);
    for(;;){
        UINT8 k = KIND(here);
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { cur = cur ? cur - 1 : G_N - 1; msg = 0; sfx(SFX_MOVE); mk_draw(cur, msg); }
        if(HIT(J_DOWN)) { cur = cur < G_N - 1 ? cur + 1 : 0; msg = 0; sfx(SFX_MOVE); mk_draw(cur, msg); }
        if(HIT(J_B) || HIT(J_START)) break;
        if(held_hit(J_RIGHT)){
            n = econ_buy(here, k, cur, 1);
            if(n){ msg = 0; sfx(SFX_COIN); } else { msg = econ_load() >= HOLD_CAP() ? "THE HOLD IS FULL" : "NOT ENOUGH GOLD"; sfx(SFX_BACK); }
            mk_draw(cur, msg);
        }
        if(held_hit(J_LEFT)){
            n = econ_sell_n(here, k, cur, 1);
            if(n){ msg = 0; sfx(SFX_COIN); } else { msg = "NONE TO SELL"; sfx(SFX_BACK); }
            mk_draw(cur, msg);
        }
        if(HIT(J_A)){
            n = econ_buy(here, k, cur, 99);
            if(n){ char b[20], *d = num(b, "BOUGHT ", n, ""); msg = b; sfx(SFX_COIN); mk_draw(cur, msg); }
            else { msg = econ_load() >= HOLD_CAP() ? "THE HOLD IS FULL" : "NOT ENOUGH GOLD"; sfx(SFX_BACK); mk_draw(cur, msg); }
        }
        if(HIT(J_SELECT)){
            UINT8 had = sv.el.cargo[cur];
            n = econ_sell_n(here, k, cur, 255);
            if(n){ char b[20]; num(b, "SOLD ", n, ""); msg = b; sfx(SFX_COIN); mk_draw(cur, msg); }
            else { msg = had ? "" : "NONE TO SELL"; sfx(SFX_BACK); mk_draw(cur, msg); }
        }
    }
}

/* ---------------------------------------------------------------- fuel and repairs */
static void sv_draw(UINT8 cur, const char *msg){
    char b[24], *d; UINT8 need = TANK_CAP() > sv.el.fuel ? TANK_CAP() - sv.el.fuel : 0; UINT16 mx = hud_hpmax(sv.level), miss = mx > sv.hp ? mx - sv.hp : 0;
    txt_win(1);
    txt_line(1, 1, "SERVICE", 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    d = scat(b, "GOLD "); d = u32s(d, sv.gold > 999999999UL ? 999999999UL : sv.gold); pad(d, b, 18); txt_line(1, 2, b, 18);
    d = scat(b, cur == 0 ? S_CURSOR : " "); d = scat(d, "REFUEL"); d = pad(d, b, 9); d = num(d, "", sv.el.fuel, "/"); d = u16s(d, TANK_CAP()); pad(d, b, 18); txt_line(1, 4, b, 18);
    d = scat(b, cur == 1 ? S_CURSOR : " "); d = scat(d, "REPAIR"); d = pad(d, b, 9); d = u16s(d, sv.hp); d = scat(d, "/"); d = u16s(d, mx); pad(d, b, 18); txt_line(1, 5, b, 18);
    d = scat(b, cur == 2 ? S_CURSOR : " "); d = scat(d, "FILL BOTH"); pad(d, b, 18); txt_line(1, 6, b, 18);
    d = num(b, "FUEL ", need, " UNITS: "); d = u16s(d, disc_price(FUEL_PRICE) * need); pad(d, b, 18); txt_line(1, 9, b, 18);
    d = num(b, "REPAIR ", miss, " HP: "); d = u16s(d, disc_price(2) * miss); pad(d, b, 18); txt_line(1, 10, b, 18);
    txt_line(1, 12, "1 FUEL = 1 CELL", 18); txt_color(1, 12, 18, 1, TXT_WHITE);
    txt_line(1, 13, "OF HYPERJUMP", 18); txt_color(1, 13, 18, 1, TXT_WHITE);
    txt_line(1, 15, msg ? msg : "", 18); txt_color(1, 15, 18, 1, TXT_RED);
    txt_win(0);
}
static UINT8 do_fuel(void){
    UINT8 need = TANK_CAP() > sv.el.fuel ? TANK_CAP() - sv.el.fuel : 0, can; UINT16 p = disc_price(FUEL_PRICE);
    if(!need) return 0;
    can = (UINT8)(sv.gold / p > need ? need : sv.gold / p);
    sv.gold -= (UINT32)p * can; sv.el.fuel += can;
    return can;
}
static UINT16 do_repair(void){
    UINT16 mx = hud_hpmax(sv.level), miss = mx > sv.hp ? mx - sv.hp : 0, p = disc_price(2), can;
    if(!miss) return 0;
    can = (UINT16)(sv.gold / p > miss ? miss : sv.gold / p);
    sv.gold -= (UINT32)p * can; sv.hp += can;
    return can;
}
static void service(void){
    UINT8 cur = 0; const char *msg = 0;
    frame_clear(); sv_draw(cur, msg);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { cur = cur ? cur - 1 : 2; msg = 0; sfx(SFX_MOVE); sv_draw(cur, msg); }
        if(HIT(J_DOWN)) { cur = cur < 2 ? cur + 1 : 0; msg = 0; sfx(SFX_MOVE); sv_draw(cur, msg); }
        if(HIT(J_B) || HIT(J_START)) break;
        if(HIT(J_A)){
            UINT8 f = 0; UINT16 r = 0;
            if(cur == 0 || cur == 2) f = do_fuel();
            if(cur == 1 || cur == 2) r = do_repair();
            if(f || r){ msg = "DONE. THANK YOU!"; sfx(SFX_COIN); }
            else {
                UINT8 ff = sv.el.fuel >= TANK_CAP(), hf = sv.hp >= hud_hpmax(sv.level);
                if(cur == 0) msg = ff ? "TANK IS FULL" : "NOT ENOUGH GOLD";
                else if(cur == 1) msg = hf ? "NO REPAIRS NEEDED" : "NOT ENOUGH GOLD";
                else msg = (ff && hf) ? "ALL FULL ALREADY" : "NOT ENOUGH GOLD";
                sfx(SFX_BACK);
            }
            sv_draw(cur, msg);
        }
    }
}

/* ---------------------------------------------------------------- directions */
static char *where(char *d, UINT8 from, UINT8 to){                  /* "3E 2N": the short way round */
    INT8 dx = (INT8)(to & 15) - (INT8)(from & 15), dy = (INT8)(to >> 4) - (INT8)(from >> 4);
    if(dx > 8) dx -= 16; if(dx < -8) dx += 16;
    if(dy > 8) dy -= 16; if(dy < -8) dy += 16;
    if(dx){ d = u16s(d, dx < 0 ? -dx : dx); *d++ = dx < 0 ? 'W' : 'E'; *d++ = ' '; }
    if(dy){ d = u16s(d, dy < 0 ? -dy : dy); *d++ = dy < 0 ? 'N' : 'S'; }
    *d = 0; return d;
}

/* ---------------------------------------------------------------- the job board */
static void jb_draw(UINT8 kind, UINT8 n, UINT8 dest, UINT16 rew, UINT8 active, const char *msg){
    char b[40], *d;
    txt_win(1);
    txt_line(1, 1, "JOB BOARD", 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    txt_line(1, 3, active ? "YOUR JOB:" : "A JOB FOR YOU:", 18); txt_color(1, 3, 18, 1, TXT_WHITE);
    if(kind == J_HUNT){
        d = num(b, "SHOOT DOWN ", n, " PIRATES"); pad(d, b, 18); txt_line(1, 5, b, 18);
        if(active){ d = num(b, "DONE: ", sv.el.jp, "/"); d = u16s(d, n); pad(d, b, 18); } else scat(b, "ANYWHERE IN SPACE");
        txt_line(1, 6, b, 18);
    } else {
        d = scat(b, "BRING A PARCEL TO"); txt_line(1, 5, b, 18);
        d = world_name(b, world_seed(dest)); pad(d, b, 18); txt_line(1, 6, b, 18); txt_color(1, 6, 18, 1, TXT_GOLD);
        d = where(b, here, dest); d = scat(d, "  FUEL "); d = u16s(d, jump_fuel(here, dest)); pad(d, b, 18); txt_line(1, 7, b, 18);
    }
    d = num(b, "PAYS ", rew, " GOLD"); pad(d, b, 18); txt_line(1, 9, b, 18); txt_color(1, 9, 18, 1, TXT_GOLD);
    txt_line(1, 11, "PAID WHEN DONE AT", 18);
    txt_line(1, 12, kind == J_HUNT ? "ANY STATION" : "THAT STATION", 18);
    txt_line(1, 14, active ? "A: GIVE UP JOB" : "A: TAKE THE JOB", 18); txt_color(1, 14, 18, 1, TXT_WHITE);
    txt_line(1, 15, "B: BACK", 18);
    txt_line(1, 16, msg ? msg : "", 18); txt_color(1, 16, 18, 1, TXT_RED);
    txt_win(0);
}
static void jobs(void){
    UINT8 kind, n, dest; UINT16 rew; const char *msg = 0;
    frame_clear();
    for(;;){
        UINT8 active = sv.el.jk != J_NONE;
        if(active){ kind = sv.el.jk; n = sv.el.jn; dest = sv.el.jd; rew = sv.el.jrew; } else job_offer(here, &kind, &n, &dest, &rew);
        jb_draw(kind, n, dest, rew, active, msg);
        for(;;){
            wait_vbl_done(); input_poll();
            if(HIT(J_B) || HIT(J_START)) return;
            if(HIT(J_A)){
                if(active){ job_take(J_NONE, 0, 0, 0); msg = "JOB DROPPED"; sfx(SFX_BACK); }
                else { job_take(kind, n, dest, rew); msg = "GOOD LUCK, PILOT!"; sfx(SFX_OK); }
                break;
            }
        }
    }
}

/* ---------------------------------------------------------------- the lounge: tips that are true */
static void lounge(void){
    UINT8 k = KIND(here), g, best_g = 0, best_c = 0, c, dx, dy; INT16 best = -999; char b[100], *d;
    for(dy = 0; dy < 9; dy++) for(dx = 0; dx < 9; dx++){
        c = (UINT8)((((here >> 4) + dy + 12) & 15) << 4 | (((here & 15) + dx + 12) & 15));
        if(c == here) continue;
        for(g = 0; g < G_N; g++){
            INT16 pr = (INT16)econ_sell(c, KIND(c), g) - (INT16)econ_price(here, k, g);
            if(pr > best){ best = pr; best_g = g; best_c = c; }
        }
    }
    d = scat(b, "BUY "); d = good_at(d, best_g); d = scat(d, " HERE FOR "); d = u16s(d, econ_price(here, k, best_g)); d = scat(d, ". ");
    d = world_name(d, world_seed(best_c)); d = scat(d, " PAYS "); d = u16s(d, econ_sell(best_c, KIND(best_c), best_g)); d = scat(d, ". THAT IS ");
    d = where(d, here, best_c); d = scat(d, " AWAY (FUEL "); d = u16s(d, jump_fuel(here, best_c)); scat(d, ").");
    dlg_say("BARTENDER", b);
    if(sv.cores < 8){ d = num(b, "THE DRONES HOLD ", 8 - sv.cores, " DATA CORES. FREE THE WORLDS, AND THE TECHNOBEAST WILL COME OUT TO PLAY."); dlg_say("OLD MOTH", b); }
    else dlg_say("OLD MOTH", "THE TECHNOBEAST SLEEPS AT THE MIDDLE OF SPACE. BRING FRIENDS. BRING LASERS.");
}

/* ---------------------------------------------------------------- the dock */
static void dock_draw(UINT8 sel, const char *msg){
    static const char *const item[6] = { "MARKET", "SERVICE", "SHIPYARD", "JOB BOARD", "LOUNGE", "UNDOCK" };
    UINT8 i; char b[24], *d;
    txt_win(1);
    d = world_name(b, world_seed(here)); d = scat(d, " STN"); pad(d, b, 18); txt_line(1, 1, b, 18); txt_color(1, 1, 18, 1, TXT_GOLD);
    d = scat(b, kind_name[KIND(here)]); d = scat(d, " WORLD"); pad(d, b, 18); txt_line(1, 2, b, 18); txt_color(1, 2, 18, 1, TXT_WHITE);
    d = scat(b, "RANK "); d = rank_at(d, fame_rank()); pad(d, b, 18); txt_line(1, 3, b, 18);
    d = scat(b, "GOLD "); d = u32s(d, sv.gold > 999999999UL ? 999999999UL : sv.gold); pad(d, b, 18); txt_line(1, 4, b, 18);
    d = num(b, "FUEL ", sv.el.fuel, "/"); d = u16s(d, TANK_CAP()); pad(d, b, 18); txt_line(1, 5, b, 18);
    d = num(b, "HOLD ", econ_load(), "/"); d = u16s(d, HOLD_CAP()); pad(d, b, 18); txt_line(1, 6, b, 18);
    d = num(b, "HP ", sv.hp, "/"); d = u16s(d, hud_hpmax(sv.level)); pad(d, b, 18); txt_line(1, 7, b, 18);
    for(i = 0; i < 6; i++){ d = scat(b, i == sel ? S_CURSOR : " "); d = scat(d, item[i]); pad(d, b, 18); txt_line(1, 8 + i, b, 18); }
    if(sv.el.jk == J_HUNT){ d = num(b, "JOB: HUNT ", sv.el.jp, "/"); d = u16s(d, sv.el.jn); pad(d, b, 18); }
    else if(sv.el.jk == J_SEND){ d = scat(b, "JOB: GO TO "); d = world_name(d, world_seed(sv.el.jd)); pad(d, b, 18); }
    else scat(b, "NO JOB");
    txt_line(1, 15, b, 18); txt_color(1, 15, 18, 1, TXT_GOLD);
    txt_line(1, 16, msg ? msg : "", 18); txt_color(1, 16, 18, 1, TXT_RED);
    txt_win(0);
}

void elite_dock(UINT8 star) BANKED {
    UINT8 sel = 0, sprites = LCDC_REG & LCDCF_OBJON; char b[32], *d; const char *msg = 0;
    here = star;
    HIDE_SPRITES; sfx(SFX_STAIRS);
    sv.mp = hud_mpmax(sv.level);                                     /* the shield recharges on the dock */
    if(job_done(here)){ UINT16 g = job_pay(); d = num(b, "JOB DONE! +", g, " GOLD"); msg = b; sfx(SFX_FANFARE); }
    frame_clear(); dock_draw(sel, msg);
    for(;;){
        wait_vbl_done(); input_poll();
        if(HIT(J_UP))   { sel = sel ? sel - 1 : 5; msg = 0; sfx(SFX_MOVE); dock_draw(sel, msg); }
        if(HIT(J_DOWN)) { sel = sel < 5 ? sel + 1 : 0; msg = 0; sfx(SFX_MOVE); dock_draw(sel, msg); }
        if(HIT(J_B) || HIT(J_START)) break;
        if(HIT(J_A)){
            sfx(SFX_OK);
            if(sel == 5) break;
            if(sel == 0) market(); else if(sel == 1) service(); else if(sel == 2) shop_run(1); else if(sel == 3) jobs(); else lounge();
            frame_clear(); msg = 0; dock_draw(sel, msg);
        }
    }
    sfx(SFX_BACK);
    if(opt.autosave) slot_write(cur_slot);
    txt_win(1); txt_cls(); txt_win(0);
    if(sprites) SHOW_SPRITES;
}
