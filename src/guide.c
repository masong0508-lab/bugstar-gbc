#pragma bank 22
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "sound.h"
#include "input.h"
#include "save.h"
#include "text.h"
#include "menus.h"
#include "guide.h"
#include "guide_pages.h"

#define IC_BUG 0
static const INT8 bob[4] = { 0, -1, -2, -1 };

static void page_draw(UINT8 p){
    UINT8 i, k; char b[24]; palette_color_t pb[16];
    txt_cls();
    { char *d = scat(b, "HOW TO PLAY  "); d = u16s(d, p + 1); d = scat(d, "/"); u16s(d, GUIDE_N); }
    txt_line(0, 0, b, 20); txt_color(0, 0, 20, 1, TXT_GREY);
    txt_line(0, 1, gd_title[p], 20); txt_color(0, 1, 20, 1, TXT_GOLD);
    for(i = 0; i < 9; i++) txt_line(0, 3 + i, gd_line[p][i], 20);
    txt_color(0, 3, 20, 9, TXT_WHITE);
    for(k = 0; k < GUIDE_N; k++){ char c[2]; c[0] = 0x1D; c[1] = 0; txt_put(1 + k, 16, c); txt_color(1 + k, 16, 1, 1, k == p ? TXT_GOLD : TXT_GREY); }
    txt_line(0, 17, "< >: PAGE   B: BACK", 20); txt_color(0, 17, 20, 1, TXT_GREY);
    for(k = 0; k < 4; k++) memcpy(&pb[k * 4], &guide_pals[gd_icon[p][k] * 4], 8);
    set_sprite_palette(0, 4, pb);
    for(k = 0; k < 4; k++){
        UINT8 t = (UINT8)(128 + gd_icon[p][k] * 4);
        set_sprite_tile(2 * k, t); set_sprite_tile(2 * k + 1, t + 2);
        set_sprite_prop(2 * k, k); set_sprite_prop(2 * k + 1, k);
    }
}

void guide_run(void) BANKED {
    UINT8 p = 0, t = 0, k, was = LCDC_REG & LCDCF_OBJ16, crawl, cx = 0;
    crawl = (((frames32() * 2654435761UL) >> 12) % 1000UL) == 0;       /* a little bug trots along the bottom, 1 time in 1000 */
    VBK_REG = 0;
    for(k = 0; k < 40; k++) move_sprite(k, 0, 0);
    DISPLAY_OFF; SPRITES_8x16;
    set_sprite_data(128, 96, guide_tiles);
    txt_pal_reset();
    if(crawl){ set_sprite_palette(4, 1, (const palette_color_t *)&guide_pals[IC_BUG * 4]); set_sprite_tile(8, 128 + IC_BUG * 4); set_sprite_tile(9, 130 + IC_BUG * 4); set_sprite_prop(8, 4); set_sprite_prop(9, 4); }
    page_draw(p);
    SHOW_BKG; SHOW_SPRITES; DISPLAY_ON;
    for(;;){
        wait_vbl_done(); input_poll(); t++;
        for(k = 0; k < 4; k++){
            INT8 y = 104 + bob[((t >> 3) + k) & 3];
            move_sprite(2 * k, 24 + k * 36 + 8, y + 16); move_sprite(2 * k + 1, 24 + k * 36 + 16, y + 16);
        }
        if(crawl){ if(!(t & 1)) cx++; move_sprite(8, cx + 8, 128 + 16 + ((t >> 3) & 1)); move_sprite(9, cx + 16, 128 + 16 + ((t >> 3) & 1)); if(cx > 170) cx = 0; }
        if((HIT(J_RIGHT) || HIT(J_A)) && p < GUIDE_N - 1){ p++; sfx(SFX_MOVE); page_draw(p); }
        else if(HIT(J_A)) break;
        if(HIT(J_LEFT) && p > 0){ p--; sfx(SFX_MOVE); page_draw(p); }
        if(HIT(J_B) || HIT(J_START)) break;
    }
    for(k = 0; k < 40; k++) move_sprite(k, 0, 0);
    HIDE_SPRITES; if(!was) SPRITES_8x8;
    txt_cls();
}
