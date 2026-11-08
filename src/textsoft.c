#pragma bank 41
/* BUGSTAR GBC - flicker-free text redraw (see txt_soft in text.h).
   The shadow is a 20x18 copy of one text layer (BG or window): the tile numbers and the palette bytes, in WRAM bank 6 (nothing else uses it).
   txt_soft() blanks the shadow, the txt_* calls fill it in, txt_soft_end() reads video memory back row by row and writes only what differs. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "text.h"
#include "font.h"

#define SW 20
#define SH 18
extern UINT8 txt_to_win, txt_soft_win;
#ifdef __SDCC
#define SHT ((UINT8 *)0xD000)
#define SHA ((UINT8 *)0xD200)
#define WRAM6() { o = SVBK_REG; SVBK_REG = 6; }
#define WRAMX() { SVBK_REG = o; }

UINT8 txt_soft_put(UINT8 x, UINT8 y, UINT8 w, UINT8 h, const UINT8 *t, UINT8 attr) BANKED {
    UINT8 o, r, c; UINT8 *d;
    if(x + w > SW || y + h > SH) return 0;                  /* (partly off the screen: the caller writes it straight) */
    WRAM6();
    d = (attr ? SHA : SHT) + (UINT16)y * SW + x;
    for(r = 0; r < h; r++){
        if(attr){ for(c = 0; c < w; c++) d[c] = *t; }       /* txt_color: one palette for the rectangle */
        else { for(c = 0; c < w; c++) d[c] = *t++; }
        d += SW;
    }
    WRAMX();
    return 1;
}
void txt_soft(void) BANKED {
    UINT8 o; UINT16 i;
    if(!(LCDC_REG & 0x80)){ txt_cls(); return; }            /* LCD off: nobody can see a blank, so clear for real */
    WRAM6();
    for(i = 0; i < SW * SH; i++){ SHT[i] = T_BLANK; SHA[i] = 0; }
    WRAMX();
    txt_soft_win = txt_to_win; txt_soft_on = 1;
}
void txt_soft_end(void) BANKED {
    UINT8 o, vb, pass, y, x, a, b, row[SW]; UINT8 *sh;
    if(!txt_soft_on) return;
    txt_soft_on = 0;
    vb = VBK_REG;
    WRAM6();
    for(pass = 0; pass < 2; pass++){
        VBK_REG = pass;
        sh = pass ? SHA : SHT;
        for(y = 0; y < SH; y++, sh += SW){
            if(txt_soft_win) get_win_tiles(0, y, SW, 1, row); else get_bkg_tiles(0, y, SW, 1, row);
            a = 255; b = 0;
            for(x = 0; x < SW; x++){
                if(row[x] != sh[x]){ if(a == 255) a = x; b = x; }
            }
            if(a != 255){
                if(txt_soft_win) set_win_tiles(a, y, b - a + 1, 1, sh + a); else set_bkg_tiles(a, y, b - a + 1, 1, sh + a);
            }
        }
    }
    VBK_REG = vb;
    WRAMX();
}
#else
UINT8 txt_soft_put(UINT8 x, UINT8 y, UINT8 w, UINT8 h, const UINT8 *t, UINT8 attr) BANKED { return 0; }
void txt_soft(void) BANKED { txt_cls(); }
void txt_soft_end(void) BANKED { txt_soft_on = 0; }
#endif
