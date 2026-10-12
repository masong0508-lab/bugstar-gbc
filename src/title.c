/* Title screen (step 2): full-screen planet art from tools/img2gbc.py.
   Art tiles live in VRAM bank 1 (bank 0 keeps the printf font), the tile map is in bank 0,
   per-tile palette/bank attributes in the bank-1 map. Menu text is baked into the art;
   the cursor is a sprite. Stars twinkle by rewriting one palette colour. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <gb/font.h>
#include <stdio.h>
#include "input.h"
#include "title.h"
#include "title_art.h"

#define CURSOR_TILE 254                 /* high sprite tile: clear of the font in tiles 0..127 */
#define MENU_Y0     102                 /* pixel row of the first menu line (see tools/make_title.py) */
#define MENU_DY     12

static const palette_color_t cursor_pal[4] = { RGB(0,0,0), RGB(2,1,6), RGB(31,26,11), RGB(31,31,31) };
/* text-mode BG palette 0 (printf font): deep-space blue, bug-red, gold, white */
static const palette_color_t text_pal[4]   = { RGB(1,1,6), RGB(26,6,6), RGB(30,24,4), RGB(31,31,31) };

void title_init(void){
    DISPLAY_OFF;
    set_bkg_palette(0, 8, title_pal);
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, TITLE_NTILES, title_tiles);       /* art tiles -> bank 1 */
    set_bkg_tiles(0, 0, TITLE_W, TITLE_H, title_attr); /* attributes -> bank 1 map */
    VBK_REG = VBK_BANK_0;
    set_bkg_tiles(0, 0, TITLE_W, TITLE_H, title_map);  /* tile numbers -> bank 0 map */
    set_sprite_palette(0, 1, cursor_pal);
    set_sprite_data(CURSOR_TILE, 1, title_cursor);
    set_sprite_tile(0, CURSOR_TILE);
    move_sprite(0, 54, MENU_Y0 + 16);
    SHOW_BKG; SHOW_SPRITES;
    DISPLAY_ON;
}

static void twinkle(UINT8 t){
    static palette_color_t p[4];
    UINT8 k, i, ph;
    if(t & 7) return;                                  /* every 8 frames */
    for(k = 0; k < TITLE_NSTAR; k++){
        for(i = 0; i < 4; i++) p[i] = title_pal[title_star_pal[k] * 4 + i];
        ph = ((t >> 3) + k * 5) & 15;
        if(ph == 0)                p[3] = RGB(15,13,20);   /* dim */
        else if(ph == 1 || ph == 15) p[3] = RGB(24,23,28); /* half */
        set_bkg_palette(title_star_pal[k], 1, p);
    }
}

int title_run(void){
    int sel = 0;
    UINT8 t = 0;
    for(;;){
        wait_vbl_done();
        input_poll();
        twinkle(t);
        t++;
        if(HIT(J_UP) || HIT(J_DOWN)) sel ^= 1;
        move_sprite(0, 54 + ((t >> 4) & 1), MENU_Y0 + sel * MENU_DY + 16);
        if(HIT(J_START) || HIT(J_A)) return sel;
    }
}

void title_exit(void){
    DISPLAY_OFF;
    HIDE_SPRITES;
    VBK_REG = VBK_BANK_1;
    fill_bkg_rect(0, 0, 32, 32, 0);                    /* clear attributes: palette 0, bank 0 */
    VBK_REG = VBK_BANK_0;
    set_bkg_palette(0, 1, text_pal);
    font_init();
    font_set(font_load(font_ibm));                     /* back to printf text mode */
    cls();
    DISPLAY_ON;
}
