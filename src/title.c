#pragma bank 2
/* Title screen (step 2): full-screen planet art from tools/img2gbc.py.
   Art tiles live in VRAM bank 1 (bank 0 keeps the text font, see text.c), the tile map is in bank 0,
   per-tile palette/bank attributes in the bank-1 map. Menu text is baked into the art;
   the cursor is a sprite. Stars twinkle by rewriting one palette colour.
   Style pass: the logo waves (an HBlank interrupt, raster.c), its gold glints now and then, shooting stars cross the sky and
   Bolt flies past in front of the planet, puffing smoke. Sprites are 8x16 here (bank-0 tiles 200..255, clear of the font). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "fx.h"
#include "sound.h"
#include "input.h"
#include "title.h"
#include "title_art.h"
#include "text.h"
#include "raster.h"
#include "space.h"
#include "space_art.h"

#define CURSOR_TILE 254                 /* high sprite tile: clear of the font in tiles 0..127 */
#define MENU_Y0     90                  /* pixel row of the first menu line (see tools/make_title.py) */
#define MENU_DY     9
unsigned int title_frames;

static const palette_color_t cursor_pal[4] = { RGB(0,0,0), RGB(2,1,6), RGB(31,26,11), RGB(31,31,31) };
static const palette_color_t sky_pal[4] = { RGB(0,0,0), RGB(10,12,22), RGB(22,24,31), RGB(31,31,31) };   /* shooting stars, smoke */
#define SHIP_T  200              /* sprite tiles (VRAM bank 0) */
#define PUFF_T  204
#define STAR_T  206
#define OAM_SHIP 1               /* 1,2 */
#define OAM_PUFF 3               /* 3..6 */
#define OAM_STAR 7
static const UINT8 blank16[16] = { 0 };
static const UINT8 fx_tiles[64] = {                     /* a smoke puff (8x16: a round dot at the top), then a shooting star */
    0x00,0x00, 0x18,0x18, 0x3C,0x24, 0x7E,0x5A, 0x7E,0x66, 0x3C,0x3C, 0x18,0x18, 0x00,0x00,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0x01,0x00, 0x02,0x00, 0x04,0x00, 0x00,0x08, 0x00,0x10, 0x20,0x20, 0xC0,0xC0, 0xC0,0xC0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static const palette_color_t glint[9] = { RGB(31,26,11), RGB(31,27,13), RGB(31,28,16), RGB(31,29,19), RGB(31,30,22),
                                          RGB(31,31,25), RGB(31,31,28), RGB(31,31,30), RGB(31,31,31) };

void title_init(void) BANKED {
    music(MUS_TITLE);
    DISPLAY_OFF;
    set_bkg_palette(0, 8, title_pal);
    VBK_REG = VBK_BANK_1;
    set_bkg_data(0, TITLE_NTILES, title_tiles);       /* art tiles -> bank 1 */
    set_bkg_tiles(0, 0, TITLE_ART_W, TITLE_ART_H, title_attr); /* attributes -> bank 1 map */
    VBK_REG = VBK_BANK_0;
    set_bkg_tiles(0, 0, TITLE_ART_W, TITLE_ART_H, title_map);  /* tile numbers -> bank 0 map */
    set_sprite_palette(0, 1, cursor_pal);
    set_sprite_palette(2, 1, sky_pal);
    set_sprite_data(CURSOR_TILE, 1, title_cursor);
    set_sprite_data(CURSOR_TILE + 1, 1, blank16);             /* (8x16 sprites: the arrow's lower half is empty) */
    set_sprite_data(PUFF_T, 4, fx_tiles);
    title_ship_load(SHIP_T); title_pal_load(1);
    set_sprite_tile(0, CURSOR_TILE); set_sprite_prop(0, 0);
    set_sprite_tile(OAM_SHIP, SHIP_T); set_sprite_tile(OAM_SHIP + 1, SHIP_T + 2); set_sprite_prop(OAM_SHIP, 1); set_sprite_prop(OAM_SHIP + 1, 1);
    { UINT8 i; for(i = OAM_PUFF; i <= OAM_STAR; i++){ set_sprite_tile(i, i == OAM_STAR ? STAR_T : PUFF_T); set_sprite_prop(i, 2); } }
    move_sprite(0, 54, MENU_Y0 + MENU_DY + 16);
    SPRITES_8x16; { UINT8 i; for(i = 1; i < 40; i++) move_sprite(i, 0, 0); }
    SHOW_BKG; SHOW_SPRITES;
    wave_start();
    fade_in();
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

static UINT8 ship_t, star_t, star_x, star_y, puff_x[4], puff_y[4], puff_life[4];
static UINT16 rng = 0xACE1;
static UINT8 rnd(void){ rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; return (UINT8)rng; }
static void show_off(UINT8 t){                                   /* the logo, the stars and Bolt: everything that moves on its own */
    UINT8 i, g = t & 255;
    if(t & 1) wave_tick();
    if(g < 17) set_bkg_palette_entry(0, 3, glint[g < 9 ? g : 16 - g]);     /* the logo's gold glints, every 256 frames */
    if(star_t){                                                 /* a shooting star, down and to the left */
        star_t--; star_x -= 3; star_y += 2;
        move_sprite(OAM_STAR, star_x, star_y);
        if(!star_t) move_sprite(OAM_STAR, 0, 0);
    } else if(rnd() < 2){ star_t = 18; star_x = 110 + (rnd() & 63); star_y = 12 + (rnd() & 15); }
    if(ship_t){                                                 /* Bolt flies by in front of the planet */
        UINT8 x = (UINT8)(200 - ship_t), y = 82 + ((ship_t >> 4) & 1);
        ship_t--;
        move_sprite(OAM_SHIP, x, y); move_sprite(OAM_SHIP + 1, x + 8, y);
        if(!(ship_t & 7)){ UINT8 k = (ship_t >> 3) & 3; puff_x[k] = x - 2; puff_y[k] = y + 4; puff_life[k] = 30; }
        if(!ship_t){ move_sprite(OAM_SHIP, 0, 0); move_sprite(OAM_SHIP + 1, 0, 0); }
    } else if(g == 90) ship_t = 200;
    for(i = 0; i < 4; i++){
        if(puff_life[i]){ puff_life[i]--; if(!(puff_life[i] & 3)) puff_y[i]--; move_sprite(OAM_PUFF + i, puff_x[i], puff_life[i] > 2 ? puff_y[i] : 0); }
    }
}

int title_run(UINT8 have_save) BANKED {
    UINT8 t = 0;
    int i;
    if(!visited){ visited = 1; title_sel = have_save ? TI_CONT : TI_NEW; }   /* first time: start on CONTINUE if there is a save */
    if(!enabled(title_sel, have_save)) title_sel = TI_NEW;
    title_frames = 0;
    for(;;){
        wait_vbl_done();
        input_poll();
        twinkle(t); show_off(t);
        t++; title_frames++;
        if(HIT(J_DOWN)){ i = title_sel; do { i = (i + 1) % 5; } while(!enabled(i, have_save)); title_sel = i; sfx(SFX_MOVE); }
        if(HIT(J_UP))  { i = title_sel; do { i = (i + 4) % 5; } while(!enabled(i, have_save)); title_sel = i; sfx(SFX_MOVE); }
        move_sprite(0, 46 + 8 + ((t >> 4) & 1), MENU_Y0 + title_sel * MENU_DY + 16);
        if(HIT(J_START) || HIT(J_A)){ sfx(SFX_OK); return title_sel; }
    }
}

void title_exit(void) BANKED {
    wave_stop();
    fade_out();
    { UINT8 i; for(i = 0; i < 40; i++) move_sprite(i, 0, 0); }
    DISPLAY_OFF;
    HIDE_SPRITES;
    txt_init();                                        /* font + text palettes, blank map, attributes reset */
    DISPLAY_ON;
}
