#include <gb/gb.h>
#include <gb/cgb.h>
#include "font.h"
#include "text.h"

/* colour 0 = background, 1 = shadow / empty, 2 = accent, 3 = ink (see tools/make_font.py) */
static const palette_color_t txt_pal[16] = {
    RGB(1,1,6), RGB(26,6,6),  RGB(30,24,4),  RGB(31,31,31),   /* 0 white: bug-red shadow, gold accent */
    RGB(1,1,6), RGB(20,12,1), RGB(31,22,6),  RGB(31,30,12),   /* 1 gold */
    RGB(1,1,6), RGB(11,0,0),  RGB(31,10,6),  RGB(31,24,22),   /* 2 red */
    RGB(1,1,6), RGB(3,3,5),   RGB(11,11,14), RGB(16,16,20)    /* 3 grey */
};

static UINT8 to_win;                       /* 1: draw into the window layer instead of the background */
void txt_win(UINT8 on){ to_win = on; }
static void put_tiles(UINT8 x, UINT8 y, UINT8 w, UINT8 h, const UINT8 *t){
    if(to_win) set_win_tiles(x, y, w, h, t); else set_bkg_tiles(x, y, w, h, t);
}
static void txt_fill(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 t){
    if(to_win) fill_win_rect(x, y, w, h, t); else fill_bkg_rect(x, y, w, h, t);
}

void txt_pal_reset(void){ set_bkg_palette(0, 4, txt_pal); }

void txt_cls(void){
    VBK_REG = VBK_BANK_1; txt_fill(0, 0, 32, 32, 0);     /* attributes: palette 0, bank 0 */
    VBK_REG = VBK_BANK_0; txt_fill(0, 0, 32, 32, T_BLANK);
}

void txt_init(void){
    VBK_REG = VBK_BANK_0;
    bsfont_load();
    set_bkg_palette(0, 4, txt_pal);
    txt_cls();
}

void txt_put(UINT8 x, UINT8 y, const char *s){
    UINT8 b[32], n = 0;
    while(s[n] && n < 32){ b[n] = (UINT8)s[n] & 0x7F; n++; }
    if(n) put_tiles(x, y, n, 1, b);
}

void txt_line(UINT8 x, UINT8 y, const char *s, UINT8 w){
    UINT8 b[32], i = 0;
    if(w > 32) w = 32;
    while(i < w && s[i]){ b[i] = (UINT8)s[i] & 0x7F; i++; }
    while(i < w) b[i++] = T_BLANK;
    if(w) put_tiles(x, y, w, 1, b);
}

void txt_box(UINT8 x, UINT8 y, UINT8 w, UINT8 h){
    UINT8 b[32], r, c;
    if(w < 2 || h < 2 || w > 32) return;
    for(r = 0; r < h; r++){
        UINT8 l = r == 0 ? T_BOX_TL : r == h - 1 ? T_BOX_BL : T_BOX_L;
        UINT8 m = r == 0 ? T_BOX_T  : r == h - 1 ? T_BOX_B  : T_BLANK;
        UINT8 e = r == 0 ? T_BOX_TR : r == h - 1 ? T_BOX_BR : T_BOX_R;
        b[0] = l; for(c = 1; c < w - 1; c++) b[c] = m; b[w - 1] = e;
        put_tiles(x, y + r, w, 1, b);
    }
}

void txt_bar(UINT8 x, UINT8 y, UINT8 w, UINT16 val, UINT16 max){
    UINT8 b[32], i; UINT16 px, f;
    if(w < 3 || w > 32) return;
    px = (UINT16)(w - 2) * 8;
    if(val > max) val = max;
    f = max ? (UINT16)(((UINT32)val * px) / max) : 0;
    b[0] = T_BAR_L;
    for(i = 0; i < w - 2; i++){
        UINT16 lo = (UINT16)i * 8;
        b[1 + i] = T_BAR0 + (f <= lo ? 0 : f - lo >= 8 ? 8 : (UINT8)(f - lo));
    }
    b[w - 1] = T_BAR_R;
    put_tiles(x, y, w, 1, b);
}

void txt_color(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 pal){
    VBK_REG = VBK_BANK_1; txt_fill(x, y, w, h, pal & 7);
    VBK_REG = VBK_BANK_0;
}
