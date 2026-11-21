#pragma bank 39
/* BUGSTAR GBC - Bolt's talk (port of the GBA game's bolt_talk and its 64 idle remarks, six zone remarks, class remarks and trait remarks). */
#include <gb/gb.h>
#include <string.h>
#include "save.h"
#include "dialog.h"
#include "menus.h"
#include "hud.h"
#include "wild.h"
#include "bolt.h"
#include "bolt_data.h"

UINT8 bolt_wk;
static UINT8 bolt_chat;                                         /* how many times you have talked to him (not saved): rotates his remarks */
/* the GBA has 16 planet classes and 40 weird traits; the GBC has 6 kinds of world (GARDEN DESERT ICE JUNGLE OCEAN DUSK) and 16 weird types: each takes the nearest remark */
static const UINT8 wk_cls[12] = { 0, 4, 5, 6, 3, 9, 1, 6, 12, 10, 11, 9 };
static const UINT8 wt_trait[32] = { 19, 6, 15, 38, 35, 36, 1, 10, 30, 24, 33, 26, 27, 16, 34, 8,
                                    19, 23, 7, 13, 18, 26, 33, 10, 28, 37, 27, 13, 20, 29, 24, 36 };

/* Bolt's caller already owns a 110-byte dialogue buffer. Reusing it
   avoids a second 110-byte automatic array on the nested say->dlg_say path
   (the cartridge's fixed WRAM stack is small). Copy before crossing the
   bank boundary so ROM strings remain mapped for the whole read. */
static void say(const char *t, char *b){ strncpy(b, t, 109); b[109] = 0; dlg_say("BOLT", b); }

void bolt_talk(void) BANKED {
    char b[110], *d; UINT8 w = weird_of(sv.star), n = 0, k, cores = sv.cores > 8 ? 8 : sv.cores;
    UINT16 hmax = hud_hpmax(sv.level);
    const char *c[6];
    /* what worries him first (at most two things) */
    if((UINT16)(sv.hp * 2) < hmax){ say("YOU ARE HURT. THE CARE TAB WILL HEAL YOU.", b); n++; }
    if(sv.el.fuel * 4 < TANK_CAP() && n < 2){ say("THE FUEL IS LOW. REST HERE OR DOCK AT A STATION.", b); n++; }
    if(!sv.potions && n < 2){ say("NO HEALTH POTIONS. THE SHOP SELLS THEM.", b); n++; }
    /* what to do next */
    if(cores < 8){
        d = scat(b, "THE QUEENS HOLD MY MEMORY. "); d = u16s(d, cores); scat(d, " OF 8 ARE BACK. FIND A DOOR AND GO DOWN.");
        dlg_say("BOLT", b);
        if(cores){ k = cores - 1; d = scat(b, "I REMEMBER: "); d = scat(d, bolt_mem[k * 4]); d = scat(d, " "); scat(d, bolt_mem[k * 4 + 1]); dlg_say("BOLT", b); }
    } else if(!(sv.story & ST_DONE)){
        say("I REMEMBER MY NAME. THE DAWN. THE KING LOCKED THE REST OF MY HEART IN THE TECHNOBEAST.", b);
        say("FOLLOW THE ARROW IN SPACE. WAIT FOR ITS CORE TO GLOW, THEN FIRE. WE CAN GO HOME.", b);
    } else {
        /* Winning is not the end of caring for the rescued crew.
           Keep Bolt's familiar nickname, restored identity and the open
           galaxy together without another dialogue buffer or save flag. */
        say("I AM DAWN. BUT YOU CAN STILL CALL ME BOLT. WE CAN KEEP LOOKING FOR THE FRIENDS WE MISSED.", b);
    }
    /* a remark about where you are */
    k = 0;
    c[k++] = bolt_zone[(sv.zone % 6) * 2 + (bolt_chat & 1)];
    c[k++] = bolt_cls[wk_cls[bolt_wk % 12] * 2 + ((bolt_chat >> 1) & 1)];
    if(w){ c[k++] = "A WEIRD WORLD. KEEP YOUR EYES OPEN."; c[k++] = bolt_trait[wt_trait[w & 31]]; }
    say(c[(bolt_chat + (sv.star & 7)) % k], b);
    say(bolt_idle[(bolt_chat * 13 + cores * 5 + sv.zone * 3 + (sv.star & 63)) & 63], b);       /* 13 is odd: it visits all 64 */
    bolt_chat++;
}
