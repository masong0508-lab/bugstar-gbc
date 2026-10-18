#pragma bank 2
/* BUGSTAR GBC - step 10: the ending, after the Technobeast. Three pages of words, a line at a time, then the numbers of the
   whole game. A hurries the lines along and turns the pages. The game is saved with ST_DONE first, so CONTINUE goes on after it. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "sound.h"
#include "ending.h"

static const char *const page[3][9] = {
    { "THE TECHNOBEAST", "IS GONE.", "", "ITS HEART OPENS,", "AND EVERY SHIP", "IT EVER ATE", "FLIES FREE.", "", "" },
    { "BOLT REMEMBERS", "EVERYTHING NOW.", "", "HER NAME IS DAWN.", "SHE CARRIED THE", "SEEDS OF YOUR", "WORLDS, AND A", "SMALL BRAVE CREW.", "" },
    { "ONE OF THEM", "LOOKED LIKE YOU.", "", "THE STARS ARE", "QUIET AGAIN.", "", "THE GARDENS", "GROW.", "" } };

static UINT8 slen(const char *s){ UINT8 n = 0; while(s[n]) n++; return n; }
static void wait_a(UINT8 frames){                     /* wait, or until A */
    while(frames--){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_START)) return; }
}
static void line(UINT8 y, const char *s, UINT8 pal){ UINT8 n = slen(s); txt_put((20 - n) / 2, y, s); txt_color(0, y, 20, 1, pal); }

void ending_run(void) BANKED {
    UINT8 p, i; char b[24], *d;
    slot_write(cur_slot);
    DISPLAY_OFF; HIDE_SPRITES; HIDE_WIN; SCX_REG = 0; SCY_REG = 0; txt_pal_reset(); txt_cls(); DISPLAY_ON;
    music(MUS_ENDING);
    for(p = 0; p < 3; p++){
        txt_cls();
        for(i = 0; i < 9; i++){ line(4 + i, page[p][i], i == 0 && p == 0 ? TXT_GOLD : TXT_WHITE); if(page[p][i][0]) wait_a(40); }
        txt_put(18, 16, S_ARROW_DN);
        for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_START)) break; }
        sfx(SFX_MOVE);
    }
    txt_cls();
    line(2, "THE END", TXT_GOLD);
    d = scat(b, "THANK YOU, "); scat(d, sv.name); line(4, b, TXT_WHITE);
    d = scat(b, "WORLDS FREED "); u16s(d, sv.zone);    txt_put(3, 7, b);
    d = scat(b, "FRIENDS      "); u16s(d, sv.friends); txt_put(3, 8, b);
    d = scat(b, "LEVEL        "); u16s(d, sv.level);   txt_put(3, 9, b);
    d = scat(b, "GOLD         "); u32s(d, sv.gold);    txt_put(3, 10, b);
    d = scat(b, "TIME         "); fmt_time(d, play_secs()); txt_put(3, 11, b);
    line(14, "BUGSTAR", TXT_RED);
    line(16, "PRESS A", TXT_GREY);
    for(;;){ wait_vbl_done(); input_poll(); if(HIT(J_A) || HIT(J_START)) break; }
    sfx(SFX_OK);
    txt_cls();
}
