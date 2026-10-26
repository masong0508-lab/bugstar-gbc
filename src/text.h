#ifndef TEXT_H
#define TEXT_H
#include <gb/gb.h>
#include "font.h"
/* BUGSTAR text engine, step 4a. The font (src/font.c, made by tools/make_font.py) lives in VRAM bank 0,
   tiles 0..127, and tile number == ASCII code, so a C string is already a row of tile numbers.
   Special tiles are string macros in font.h:  txt_put(1, 2, S_HEART "HP");
   BG palettes 0..3 are text colours: 0 white, 1 gold, 2 red, 3 grey (set them per area with txt_color). */
#define TXT_WHITE 0
#define TXT_GOLD  1
#define TXT_RED   2
#define TXT_GREY  3

void txt_win(UINT8 on);                               /* 1: every txt_* call below draws into the window layer, 0: background */
void txt_init(void);                                  /* LCD must be off: loads font + text palettes, clears screen */
void txt_cls(void);                                   /* blank the whole BG map and reset every tile to palette 0 */
void txt_put(UINT8 x, UINT8 y, const char *s);        /* write s as tiles at x,y (up to 32 chars) */
void txt_line(UINT8 x, UINT8 y, const char *s, UINT8 w); /* like txt_put but space-padded/cut to exactly w tiles */
void txt_box(UINT8 x, UINT8 y, UINT8 w, UINT8 h);     /* framed box, interior cleared (w,h in tiles, both >= 2) */
void txt_bar(UINT8 x, UINT8 y, UINT8 w, UINT16 val, UINT16 max); /* bar w tiles wide (>= 3) incl. end caps */
void txt_color(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 pal);   /* recolour a rectangle with BG palette 0..7 (0..3 are the text colours) */
void txt_pal_reset(void);                             /* reload the four text palettes (after a screen borrowed them) */
/* a pulsing menu arrow: call every frame with a running frame counter f; redraws the arrow cell every 16 frames */
#define UI_BLINK(x,y,f) do{ if(!((f)&15)) txt_line((x),(y),((f)&16)?S_CURSOR:">",1); }while(0)
#endif
