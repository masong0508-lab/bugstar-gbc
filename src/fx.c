#pragma bank 7
/* BUGSTAR GBC - style: palette fades and the WARNING cut-in (see fx.h).
   The fades read the colour RAM itself, so they work on any scene: fade_in finds the palettes the scene just set (the LCD is off,
   so colour RAM can be read), blacks them out, turns the LCD on and raises them back over 8 frames. fade_out reads them (in VBlank)
   and lowers them to black. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "input.h"
#include "save.h"
#include "text.h"
#include "hud.h"
#include "sound.h"
#include "fx.h"

static UINT8 orig[128], work[128];       /* BG colour RAM bytes 0..63, then OBJ 64..127 */

static void read_bg(void){ UINT8 i; for(i = 0; i < 64; i++){ BCPS_REG = i; orig[i] = BCPD_REG; } }
static void read_obj(void){ UINT8 i; for(i = 0; i < 64; i++){ OCPS_REG = i; orig[64 + i] = OCPD_REG; } }
static void dim(UINT8 s){                 /* work = orig with every channel s darker (5-bit channels) */
    UINT8 i;
    for(i = 0; i < 128; i += 2){
        UINT16 c = orig[i] | ((UINT16)orig[i + 1] << 8); UINT8 r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
        r = r > s ? r - s : 0; g = g > s ? g - s : 0; b = b > s ? b - s : 0;
        c = r | ((UINT16)g << 5) | ((UINT16)b << 10);
        work[i] = (UINT8)c; work[i + 1] = (UINT8)(c >> 8);
    }
}
static void put_bg(void){ UINT8 i; BCPS_REG = 0x80; for(i = 0; i < 64; i++) BCPD_REG = work[i]; }
static void put_obj(void){ UINT8 i; OCPS_REG = 0x80; for(i = 0; i < 64; i++) OCPD_REG = work[64 + i]; }

void fade_in(void) BANKED {
    UINT8 s;
    read_bg(); read_obj();
    dim(31); put_bg(); put_obj();
    DISPLAY_ON;
    for(s = 28;; s -= 4){
        dim(s);
        wait_vbl_done(); put_bg(); put_obj();
        if(!s) break;
    }
}
void fade_out(void) BANKED {
    UINT8 s;
    if(!(LCDC_REG & 0x80)) return;
    wait_vbl_done(); read_bg();
    wait_vbl_done(); read_obj();
    for(s = 4; s <= 32; s += 4){
        dim(s);
        wait_vbl_done(); put_bg(); put_obj();
    }
}

static const char *const title_txt[3] = { "THE TECHNOBEAST", "THE SPIDER QUEEN", "HYPERSPACE ATTACK" };
void banner(UINT8 id) BANKED {
    UINT8 t, i, had = hud_is_on(); char tape[21];
    txt_win(1);
    txt_cls();
    txt_line(0, 1, "", 20); txt_put(4, 1, "! WARNING !");
    { const char *s = title_txt[id]; UINT8 n = 0; while(s[n]) n++; txt_line(0, 2, "", 20); txt_put((20 - n) / 2, 2, s); }
    txt_color(0, 2, 20, 1, TXT_WHITE);
    txt_win(0);
    move_win(7, 144 - 32); SHOW_WIN;                      /* over the HUD, at the bottom */
    for(t = 0; t < 96; t++){
        if(!(t & 3)){                                      /* hazard tape that crawls along, top and bottom */
            for(i = 0; i < 20; i++) tape[i] = ((i + (t >> 2)) & 2) ? 127 : ' ';
            tape[20] = 0;
            txt_win(1); txt_put(0, 0, tape); txt_put(0, 3, tape); txt_color(0, 0, 20, 1, TXT_GOLD); txt_color(0, 3, 20, 1, TXT_GOLD);
            txt_color(0, 1, 20, 1, (t & 8) ? TXT_RED : TXT_GOLD); txt_win(0);
        }
        if(!(t & 15)) sfx(SFX_HISS);
        wait_vbl_done();
    }
    if(had){ hud_redraw(); hud_show(); } else { txt_win(1); txt_cls(); txt_win(0); HIDE_WIN; }
}
