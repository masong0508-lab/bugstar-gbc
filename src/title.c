#pragma bank 2
/* Title screen (step 2): full-screen planet art from tools/img2gbc.py.
   Art tiles live in VRAM bank 1 (bank 0 keeps the text font, see text.c), the tile map is in bank 0,
   per-tile palette/bank attributes in the bank-1 map. Menu text is baked into the art;
   the cursor is a sprite. Stars twinkle by rewriting one palette colour. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "input.h"
#include "title.h"
#include "title_art.h"
#include "text.h"

#define CURSOR_TILE 254                 /* high sprite tile: clear of the font in tiles 0..127 */
#define MENU_Y0     90                  /* pixel row of the first menu line (see tools/make_title.py) */
#define MENU_DY     9
unsigned int title_frames;

static const palette_color_t cursor_pal[4] = { RGB(0,0,0), RGB(2,1,6), RGB(31,26,11), RGB(31,31,31) };

void title_init(void) BANKED {
    DISPLAY_OFF;
    set_bkg_palette(0, 8, title_pal);
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, TITLE_NTILES, title_tiles);       /* art tiles -> bank 1 */
    set_bkg_tiles(0, 0, TITLE_ART_W, TITLE_ART_H, title_attr); /* attributes -> bank 1 map */
    VBK_REG = VBK_BANK_0;
    set_bkg_tiles(0, 0, TITLE_ART_W, TITLE_ART_H, title_map);  /* tile numbers -> bank 0 map */
    set_sprite_palette(0, 1, cursor_pal);
    set_sprite_data(CURSOR_TILE, 1, title_cursor);
    set_sprite_tile(0, CURSOR_TILE);
    move_sprite(0, 54, MENU_Y0 + MENU_DY + 16);
    SPRITES_8x8; { UINT8 i; for(i = 1; i < 40; i++) move_sprite(i, 0, 0); }
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

static int title_sel = TI_NEW;
static UINT8 visited;
static UINT8 enabled(int i, UINT8 have_save){ return (i == TI_CONT || i == TI_LOAD) ? have_save : 1; }

int title_run(UINT8 have_save) BANKED {
    UINT8 t = 0;
    int i;
    if(!visited){ visited = 1; title_sel = have_save ? TI_CONT : TI_NEW; }   /* first time: start on CONTINUE if there is a save */
    if(!enabled(title_sel, have_save)) title_sel = TI_NEW;
    title_frames = 0;
    for(;;){
        wait_vbl_done();
        input_poll();
        twinkle(t);
        t++; title_frames++;
        if(HIT(J_DOWN)){ i = title_sel; do { i = (i + 1) % 4; } while(!enabled(i, have_save)); title_sel = i; }
        if(HIT(J_UP))  { i = title_sel; do { i = (i + 3) % 4; } while(!enabled(i, have_save)); title_sel = i; }
        move_sprite(0, 46 + 8 + ((t >> 4) & 1), MENU_Y0 + title_sel * MENU_DY + 16);
        if(HIT(J_START) || HIT(J_A)) return title_sel;
    }
}

void title_exit(void) BANKED {
    DISPLAY_OFF;
    HIDE_SPRITES;
    txt_init();                                        /* font + text palettes, blank map, attributes reset */
    DISPLAY_ON;
}
