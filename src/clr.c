/* BUGSTAR GBC - the COLOURS option, in bank 0 (it must be callable from anywhere, even a raster interrupt, without a bank switch).
   Every palette the game sets goes through GBDK's set_bkg_palette / set_sprite_palette / ..._entry. Those four are replaced here (the linker takes
   ours and never loads the library's cgb_palettes module), so one filter covers every scene: opt.clr 0 NORMAL, 1 HIGH CONTRAST (three levels a channel), 2 GREEN (the first Game Boy), 3 SEPIA. The mono looks are steps of luminance, and the contrast look is a posterize, so a colour that was filtered once stays put
   4 GREEN (the first Game Boy), 5 SEPIA. The mono looks are steps of luminance, and the contrast look is a posterize, so a colour that was filtered once stays put
   when it is filtered again (a scene that reads the palette back, like the fades, does no harm). */
#pragma opt_code_size
#include <gb/gb.h>
#include <gb/cgb.h>
#include "save.h"

static const UINT8 ramp_l[8] = { 195, 200, 16, 249, 68, 45, 22, 94 };
static const UINT8 ramp_h[8] = { 12, 25, 43, 75, 4, 21, 42, 79 };
static const UINT8 pt[4] = { 0, 16, 16, 31 };
#define POST(x) pt[(x) >> 3]
static UINT16 filt(UINT16 v, UINT8 m){
    UINT8 lo = (UINT8)v, hi = (UINT8)(v >> 8), r = lo & 31, g = (lo >> 5) | ((hi & 3) << 3), b = (hi >> 2) & 31, l = (r + g + g + b) >> 2;
    if(m > 1){ l = (l >> 3) + (m == 3 ? 4 : 0); return ramp_l[l] | ((UINT16)ramp_h[l] << 8); }
    r = POST(r); g = POST(g); b = POST(b);
    return (UINT8)(r | (g << 5)) | ((UINT16)((g >> 3) | (b << 2)) << 8);
}
/* n colours from first colour index `col` (0..31) into the background or sprite palette RAM; the colour RAM is only open in HBlank / VBlank / with the LCD off */
static void put(UINT8 spr, UINT8 col, const UINT16 *src, UINT8 n){
    UINT8 m = opt.clr; UINT16 v; volatile UINT8 *ix = spr ? &OCPS_REG : &BCPS_REG, *dt = ix + 1;
    CRITICAL {
        *ix = 0x80 | (col << 1);
        while(n--){
            v = *src++; if(m) v = filt(v, m);
            while(STAT_REG & STATF_BUSY);
            *dt = (UINT8)v;
            while(STAT_REG & STATF_BUSY);
            *dt = (UINT8)(v >> 8);
        }
    }
}
void set_bkg_palette(uint8_t first_palette, uint8_t nb_palettes, const palette_color_t *rgb_data) OLDCALL { put(0, first_palette << 2, rgb_data, nb_palettes << 2); }
void set_sprite_palette(uint8_t first_palette, uint8_t nb_palettes, const palette_color_t *rgb_data) OLDCALL { put(1, first_palette << 2, rgb_data, nb_palettes << 2); }
void set_bkg_palette_entry(uint8_t palette, uint8_t entry, uint16_t rgb_data) OLDCALL { put(0, (palette << 2) + entry, &rgb_data, 1); }
void set_sprite_palette_entry(uint8_t palette, uint8_t entry, uint16_t rgb_data) OLDCALL { put(1, (palette << 2) + entry, &rgb_data, 1); }
