#pragma bank 35
/* BUGSTAR GBC - the STATS book (see stats.h). */
#include <gb/gb.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "spell.h"
#include "econ.h"
#include "xtra.h"
#include "kart.h"
#include "warship.h"

static UINT8 pop(const UINT8 *a, UINT8 n){ UINT8 i, c = 0, b; for(i = 0; i < n; i++) for(b = a[i]; b; b &= b - 1) c++; return c; }
static void ln(UINT8 y, const char *lab, const char *val, UINT8 pal){        /* "LABEL ........ value" in 17 columns */
    char b[20], *d = scat(b, lab); UINT8 n = 17, l = 0, v = 0; while(b[l]) l++; while(val[v]) v++;
    while(l + v < n) b[l++] = ' ';
    d = b + l; scat(d, val); txt_line(2, y, b, 17); txt_color(2, y, 17, 1, pal);
}
static void lnu(UINT8 y, const char *lab, UINT32 v, UINT8 pal){ char t[12]; u32s(t, v); ln(y, lab, t, pal); }
static void lnf(UINT8 y, const char *lab, UINT8 of, UINT8 n, UINT8 pal){ char t[12], *d = u16s(t, n); d = scat(d, "/"); u16s(d, of); ln(y, lab, t, pal); }
static void best(UINT8 y, const char *lab, UINT16 t){                           /* a time in 60ths: m:ss.d, or "-" */
    char v[12], *d = v; UINT16 s = t / 60;
    if(!t){ ln(y, lab, "-", TXT_GREY); return; }
    d = u16s(d, s / 60); *d++ = ':'; if(s % 60 < 10) *d++ = '0'; d = u16s(d, s % 60); *d++ = '.'; d = u16s(d, (t % 60) / 6); *d = 0;
    ln(y, lab, v, TXT_GOLD);
}
static void page(UINT8 p){
    char t[16], *d; UINT8 r, i; UINT16 k;
    txt_cls(); txt_box(0, 0, 20, 18);
    txt_line(2, 1, p == 0 ? "ADVENTURE" : p == 1 ? "THE GALAXY" : "GAMES", 13); txt_color(2, 1, 13, 1, TXT_GOLD);
    if(p == 0){
        fmt_time(t, play_secs()); ln(3, "TIME", t, TXT_WHITE);
        lnu(4, "SAVES", sv.saves, TXT_WHITE);
        lnu(5, "LEVEL", sv.level, TXT_WHITE);
        lnu(6, "MONSTERS", EX_KILLS, TXT_WHITE);
        lnu(7, "SPELLS", EX_CASTS, TXT_WHITE);
        lnu(8, "FISH", EX_FISHN, TXT_WHITE);
        lnu(9, "DEEPEST FLOOR", sv.deepest, TXT_WHITE);
        lnu(10, "HELL DEPTHS", EX_HELL, TXT_WHITE);
        lnu(11, "MOON DEEP", EX_MOON, TXT_WHITE);
        lnu(12, "PIT WAVES", ex.pit, TXT_WHITE);
        lnu(13, "BUG FRIENDS", sv.friends, TXT_WHITE);
        lnu(14, "GOLD", sv.gold > 999999999UL ? 999999999UL : sv.gold, TXT_GOLD);
    } else if(p == 1){
        lnf(3, "WORLDS SEEN", 255, pop(ex.seen, 32), TXT_WHITE);
        lnf(4, "CITIES", 255, pop(ex.city, 32), TXT_WHITE);
        lnf(5, "DATA CORES", 8, sv.cores, TXT_WHITE);
        lnu(6, "JUMPS", sv.el.jumps, TXT_WHITE);
        lnu(7, "PIRATES", sv.el.kills, TXT_WHITE);
        lnu(8, "JOBS DONE", sv.el.jobs, TXT_WHITE);
        lnu(9, "FAME", sv.el.fame, TXT_WHITE);
        r = fame_rank(); t[0] = 0; rank_label(r, t); ln(10, "RANK", t, TXT_GOLD);
        lnu(11, "GOODS SOLD", sv.el.sold, TXT_GOLD);
        lnu(12, "WARP BEST", sv.warp_best, TXT_WHITE);
        lnu(13, "ARENA WAVES", ARENA_BEST, TXT_WHITE);
        lnu(14, "RELICS", xtra_relics(), TXT_WHITE);
    } else {
        k = 0; for(i = 0; i < 3; i++){ UINT16 b = kart_rec(0, i, 0), c = kart_rec(0, i, 1); if(b && (!k || b < k)) k = b; if(c && (!k || c < k)) k = c; }
        lnu(3, "KART RACES", kart_count(0), TXT_WHITE);
        lnu(4, "KART WINS", kart_count(1), TXT_WHITE);
        best(5, "BEST LAP", k);
        best(6, "MEADOW RACE", kart_rec(1, 0, 0));
        best(7, "DESERT RACE", kart_rec(1, 1, 0));
        best(8, "SNOW RACE", kart_rec(1, 2, 0));
        lnu(10, "RAIDS FLOWN", warship_count(0), TXT_WHITE);
        lnu(11, "RAIDS WON", warship_count(1), TXT_WHITE);
        d = t; d = u16s(d, ex.fish); *d = 0; ln(12, "FISH IN BAG", t, TXT_WHITE);
        lnu(13, "GOLDEN FISH", ex.rare, TXT_WHITE);
    }
    txt_line(2, 16, "< > PAGE  B: BACK", 17); txt_color(2, 16, 17, 1, TXT_GREY);
    d = t; d = scat(d, p == 0 ? "1/3" : p == 1 ? "2/3" : "3/3"); txt_line(16, 1, t, 3); txt_color(16, 1, 3, 1, TXT_GREY);
}
void stats_run(void) BANKED {
    UINT8 p = 0, d = 1;
    DISPLAY_OFF; HIDE_WIN; HIDE_SPRITES; txt_pal_reset();
    for(;;){
        if(d){ page(p); d = 0; DISPLAY_ON; }
        wait_vbl_done(); input_poll();
        if(HIT(J_B) || HIT(J_START)){ sfx(SFX_BACK); return; }
        if(HIT(J_RIGHT) || HIT(J_A)){ p = p < 2 ? p + 1 : 0; d = 1; sfx(SFX_MOVE); }
        if(HIT(J_LEFT)){ p = p ? p - 1 : 2; d = 1; sfx(SFX_MOVE); }
    }
}
