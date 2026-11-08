#pragma bank 45
/* BUGSTAR GBC - BUG HIVE (see hive.h): a small colony game in the EXTRAS, in the spirit of a kingdom builder. Six buildings make nectar, bark and rock, house
   more bugs and keep the ants out; time passes while you play (one tick every 20 seconds, even while you are away from the screen), raiders come every
   8th tick and get stronger. Sell what you have gathered for gold. Own SRAM block (offset 3300 of the slot's bank, own magic and checksum). */
#include <gb/gb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "hud.h"
#include "traits.h"
#include "hive.h"

extern volatile UINT8 snd_song;

typedef struct { UINT32 magic, seed; UINT8 lv[6]; UINT16 res[3]; UINT8 pop, won, lost, rsv; UINT16 ticks; UINT32 last; UINT32 sum; } HV;
#define HV_OFF   3300U
#define HV_MAGIC 0x31564842UL                                           /* "BHV1" */
static HV hv;
static void hv_save(void){
    const UINT8 *p = (const UINT8 *)&hv; UINT16 i;
    hv.magic = HV_MAGIC; hv.seed = sv.seed; hv.sum = fnv(&hv, sizeof hv - 4);
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof hv; i++) *(volatile UINT8 *)(0xA000U + HV_OFF + i) = p[i];
    DISABLE_RAM;
}
static void hv_load(void){
    UINT8 *p = (UINT8 *)&hv; UINT16 i;
    ENABLE_RAM; SWITCH_RAM(cur_slot);
    for(i = 0; i < sizeof hv; i++) p[i] = *(volatile UINT8 *)(0xA000U + HV_OFF + i);
    DISABLE_RAM;
    if(hv.magic != HV_MAGIC || hv.seed != sv.seed || hv.sum != fnv(&hv, sizeof hv - 4)){
        memset(&hv, 0, sizeof hv); hv.res[0] = 20; hv.res[1] = 10; hv.res[2] = 5; hv.pop = 2; hv.last = play_secs();
    }
}
UINT16 hive_info(char *out) BANKED { char *d; hv_load(); d = u16s(out, hv.pop); scat(d, " BUGS"); return hv.pop; }
void hive_reset(void) BANKED { memset(&hv, 0, sizeof hv); hv.res[0] = 20; hv.res[1] = 10; hv.res[2] = 5; hv.pop = 2; hv.last = play_secs(); hv_save(); }

enum { B_FARM, B_MILL, B_QUARRY, B_HOUSE, B_WALL, B_TOWER };
static const char *const bname[6] = { "FARM", "MILL", "QUARRY", "HOUSE", "WALL", "TOWER" };
static const char *const bwhat[6] = { "NECTAR", "BARK", "ROCK", "ROOM", "DEFENCE", "DEFENCE" };
static const UINT8 bcost[6][3] = { { 4, 2, 0 }, { 6, 0, 0 }, { 6, 4, 0 }, { 5, 4, 2 }, { 0, 6, 4 }, { 2, 8, 8 } };   /* nectar, bark, rock for level 1; times the level after that */
#define MAXLV 9
static const char *last_msg;
static UINT16 cap(UINT8 i){ return i < 3 ? 999 : 0; }
static UINT8 pop_cap(void){ return 4 + 4 * hv.lv[B_HOUSE]; }
static UINT8 defence(void){ UINT16 d = 3 * hv.lv[B_WALL] + 5 * hv.lv[B_TOWER] + hv.pop / 2; return d > 250 ? 250 : (UINT8)d; }
static UINT8 strength(void){ UINT16 s = 4 + (hv.ticks >> 3) * 2; return s > 240 ? 240 : (UINT8)s; }
static void add(UINT8 i, UINT16 v){ UINT16 n = hv.res[i] + v; hv.res[i] = n > cap(i) ? cap(i) : n; }
static void tick(void){
    UINT8 up;
    hv.ticks++;
    add(0, 1 + 2 * hv.lv[B_FARM]); add(1, hv.lv[B_MILL] * 2); add(2, hv.lv[B_QUARRY] * 2);
    up = hv.pop / 4;                                                    /* every four bugs eat one nectar */
    if(hv.res[0] >= up) hv.res[0] -= up; else { hv.res[0] = 0; if(hv.pop > 2 && !(hv.ticks & 3)) hv.pop--; }
    if(hv.pop < pop_cap() && hv.res[0] >= 2){ hv.pop++; hv.res[0] -= 2; }
    if(!(hv.ticks & 7)){                                                /* the raiders */
        if(defence() >= strength()){ hv.won++; last_msg = "RAIDERS DRIVEN OFF!"; }
        else { UINT8 i; hv.lost++; for(i = 0; i < 3; i++) hv.res[i] /= 2; if(hv.pop > 3) hv.pop -= hv.pop / 5; last_msg = "RAIDERS LOOTED THE HIVE!"; }
    }
}
static void catch_up(void){
    UINT32 now = play_secs(), n;
    if(now < hv.last){ hv.last = now; return; }
    n = (now - hv.last) / 20;
    if(n > 90) n = 90;
    hv.last += (now - hv.last) / 20 * 20; if(hv.last > now) hv.last = now;
    while(n--) tick();
}
static UINT8 can_pay(UINT8 b){ UINT8 l = hv.lv[b] + 1, i; for(i = 0; i < 3; i++) if(hv.res[i] < (UINT16)bcost[b][i] * l) return 0; return 1; }
static UINT16 sell_value(void){ UINT32 v = (UINT32)hv.res[0] + 2UL * hv.res[1] + 3UL * hv.res[2]; v = v * (4 + hv.pop) / 4; if(v > 9000UL) v = 9000UL; return tr_gold((UINT16)v) * GMULT(); }
static void draw(UINT8 sel, UINT8 again){
    UINT8 i, l; char b[24], *d;
    txt_soft(); txt_box(0, 0, 20, 18);
    d = scat(b, "BUG HIVE  MIN "); d = u16s(d, hv.ticks / 3); *d = 0; txt_line(2, 1, b, 17); txt_color(2, 1, 17, 1, TXT_GOLD);
    d = scat(b, "NECT "); d = u16s(d, hv.res[0]); d = scat(d, " BARK "); d = u16s(d, hv.res[1]); *d = 0; txt_line(2, 2, b, 17);
    d = scat(b, "ROCK "); d = u16s(d, hv.res[2]); d = scat(d, " BUG "); d = u16s(d, hv.pop); d = scat(d, "/"); d = u16s(d, pop_cap()); *d = 0; txt_line(1, 3, b, 18);
    for(i = 0; i < 6; i++){
        d = b; *d++ = i == sel ? T_CURSOR : ' '; d = scat(d, bname[i]); while(d - b < 9) *d++ = ' ';
        d = scat(d, "LV "); d = u16s(d, hv.lv[i]); *d = 0; txt_line(2, 5 + i, b, 15);
        txt_color(2, 5 + i, 15, 1, i == sel ? TXT_GOLD : TXT_WHITE);
    }
    l = hv.lv[sel];
    if(l >= MAXLV){ txt_line(2, 12, "FULLY BUILT", 17); }
    else { d = scat(b, "COST N"); d = u16s(d, bcost[sel][0] * (l + 1)); d = scat(d, " B"); d = u16s(d, bcost[sel][1] * (l + 1)); d = scat(d, " R"); d = u16s(d, bcost[sel][2] * (l + 1)); *d = 0; txt_line(2, 12, b, 17); }
    txt_color(2, 12, 17, 1, l < MAXLV && can_pay(sel) ? TXT_WHITE : TXT_GREY);
    d = scat(b, bwhat[sel]); d = scat(d, " "); *d = 0; txt_line(2, 13, b, 17); txt_color(2, 13, 17, 1, TXT_GREY);
    d = scat(b, "DEF "); d = u16s(d, defence()); d = scat(d, " VS "); d = u16s(d, strength()); d = scat(d, "  W"); d = u16s(d, hv.won); d = scat(d, " L"); d = u16s(d, hv.lost); *d = 0; txt_line(2, 14, b, 17);
    txt_color(2, 14, 17, 1, defence() >= strength() ? TXT_WHITE : TXT_RED);
    txt_line(2, 15, last_msg ? last_msg : "", 17); txt_color(2, 15, 17, 1, TXT_GOLD);
    d = scat(b, "A:BUILD SEL:SELL "); d = u16s(d, sell_value() > 9999 ? 9999 : sell_value()); *d = 0; txt_line(1, 16, b, 18); txt_color(1, 16, 18, 1, TXT_GREY);
    (void)again;
}
void hive_run(void) BANKED {
    UINT8 sel = 0, d = 1, f = 0, old = snd_song; UINT16 t = 0;
    DISPLAY_OFF; HIDE_WIN; HIDE_SPRITES; hv_load(); txt_pal_reset(); last_msg = 0;
    catch_up(); music(MUS_CAMP);
    for(;;){
        if(d){ draw(sel, d == 2); DISPLAY_ON; d = 0; }
        wait_vbl_done(); input_poll();
        f++; if(++t >= 1200){ t = 0; catch_up(); d = 2; }                   /* (time keeps passing on this screen) */
        if(HITR(J_UP)){ sel = sel ? sel - 1 : 5; last_msg = 0; d = 2; sfx(SFX_MOVE); }
        if(HITR(J_DOWN)){ sel = sel < 5 ? sel + 1 : 0; last_msg = 0; d = 2; sfx(SFX_MOVE); }
        if(HIT(J_B) || HIT(J_START)){ sfx(SFX_BACK); break; }
        if(HIT(J_A)){
            if(hv.lv[sel] >= MAXLV){ last_msg = "ALREADY FULLY BUILT"; sfx(SFX_HURT); }
            else if(!can_pay(sel)){ last_msg = "NOT ENOUGH STORES"; sfx(SFX_HURT); }
            else { UINT8 i, l = hv.lv[sel] + 1; for(i = 0; i < 3; i++) hv.res[i] -= (UINT16)bcost[sel][i] * l; hv.lv[sel]++; last_msg = "BUILT!"; sfx(SFX_OK); hv_save(); }
            d = 2;
        }
        if(HIT(J_SELECT)){
            UINT16 g = sell_value();
            if(g){ sv.gold += g; hv.res[0] = hv.res[1] = hv.res[2] = 0; last_msg = "SOLD FOR GOLD!"; sfx(SFX_COIN); hv_save(); }
            else { last_msg = "NOTHING TO SELL"; sfx(SFX_HURT); }
            d = 2;
        }
    }
    hv_save(); music(old);
    DISPLAY_OFF; HIDE_SPRITES; txt_win(0); VBK_REG = 0; txt_cls(); SCX_REG = 0; SCY_REG = 0;
}
