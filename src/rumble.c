/* BUGSTAR GBC - rumble engine (see rumble.h). Bank 0: it runs from the vblank interrupt. */
#include <gb/gb.h>
#include "save.h"
#include "rumble.h"

volatile UINT8 rum_bit, rum_bank, sram_open;
typedef struct { UINT8 amp, len, cls, shape; } RPat;
static const RPat rum_pat[NRP] = {
    { 90, 3, 1, 1 }, { 150, 5, 1, 1 }, { 45, 2, 0, 0 }, { 110, 8, 1, 1 }, { 255, 14, 2, 1 },      /* BITE HIT PICK FIRE HURT */
    { 120, 9, 2, 2 }, { 150, 30, 0, 3 }, { 35, 1, 0, 0 }, { 55, 5, 0, 1 }, { 190, 8, 1, 1 },       /* STAIRS FANFARE COIN SPLASH DIE */
    { 100, 3, 1, 1 }, { 28, 1, 0, 0 }, { 220, 8, 2, 1 }, { 140, 5, 1, 1 },                          /* SPARK STEP THUD CRUNCH */
    { 210, 70, 2, 3 }, { 255, 26, 2, 1 }, { 220, 24, 2, 3 }, { 255, 20, 2, 1 }, { 170, 22, 1, 3 }   /* QUAKE BOSS TEST CRASH CATCH */
};
static const UINT8 rum_pct[6] = { 0, 30, 60, 100, 150, 220 };     /* OFF LOW MEDIUM HIGH MAX EXTREME: how strong everything is */
static volatile UINT8 rum_id, rum_len, rum_tot, rum_hold;
static UINT8 rum_acc, rum_on;

static void motor(UINT8 on){
    rum_on = on; rum_bit = on ? 8 : 0;
    if(!sram_open) *(volatile UINT8 *)0x4000 = rum_bank | rum_bit;      /* (not while the cartridge RAM is open: a plain cart would take bit 3 for a bank) */
}
void rumble(UINT8 id){
    const RPat *p;
    if(!RUM_LVL() || id >= NRP) return;
    p = &rum_pat[id];
    if(p->cls < RUM_EV()) return;                                      /* filtered out by RUMBLE EVENTS */
    if(rum_len && (UINT16)rum_pat[rum_id].amp * rum_len / (rum_tot ? rum_tot : 1) > p->amp) return;
    rum_id = id; rum_tot = p->len; rum_len = p->len;
}
void rumble_sfx(UINT8 n){ if(n < 14) rumble(n); }                      /* SFX_BITE..SFX_CRUNCH are the first 14 patterns, in the same order */
void rum_hum(UINT8 amp){ if(RUM_HUM() && amp > rum_hold) rum_hold = amp; }
void rumble_tick(void){
    UINT8 lvl = RUM_LVL(), on = 0; UINT16 out = 0;
    if(!lvl){ if(rum_on || rum_len || rum_hold){ rum_len = 0; rum_hold = 0; rum_acc = 0; motor(0); } return; }
    if(rum_len){
        const RPat *p = &rum_pat[rum_id]; UINT8 el = rum_tot - rum_len; UINT16 e = p->amp;
        if(p->shape == 1) e = (UINT16)p->amp * rum_len / rum_tot;
        else if(p->shape == 2) e = (UINT16)p->amp * (el + 1) / rum_tot;
        else if(p->shape == 3) e = ((el >> 1) & 1) ? p->amp / 4 : p->amp;
        rum_len--; out = e;
    }
    if(rum_hold){ if(rum_hold > out) out = rum_hold; rum_hold = 0; }
    out = out * rum_pct[lvl] / 100; if(out > 255) out = 255;
    if(out) out = 64 + out * 191 / 255;                                 /* a motor will not turn on a thin pulse: squeeze the range up out of the dead zone */
    if(out){ UINT16 a = rum_acc + out; on = a >= 256; rum_acc = (UINT8)a; } else rum_acc = 0;   /* a duty cycle: the carry out of the accumulator says when the motor is on */
    if(out >= 250) on = 1;
    if(on != rum_on) motor(on);
}
