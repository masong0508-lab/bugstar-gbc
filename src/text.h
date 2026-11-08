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
/* FLICKER-FREE REDRAW. txt_soft() is txt_cls() for a screen that is already showing: nothing is blanked. Every txt_* call that follows (on the
   same BG/window target, inside the 20x18 screen) goes to a shadow copy that starts blank, and txt_soft_end() (also called by the next
   input_poll) compares the shadow with video memory and writes only the cells that differ. So a page that draws the same box and title again
   costs no write at all, and there is never a blank frame in between. With the LCD off it is a plain txt_cls(). */
extern UINT8 txt_soft_on;
void txt_soft(void) BANKED;
void txt_soft_end(void) BANKED;
void txt_put(UINT8 x, UINT8 y, const char *s);        /* write s as tiles at x,y (up to 32 chars) */
void txt_line(UINT8 x, UINT8 y, const char *s, UINT8 w); /* like txt_put but space-padded/cut to exactly w tiles */
void txt_box(UINT8 x, UINT8 y, UINT8 w, UINT8 h);     /* framed box, interior cleared (w,h in tiles, both >= 2) */
void txt_bar(UINT8 x, UINT8 y, UINT8 w, UINT16 val, UINT16 max); /* bar w tiles wide (>= 3) incl. end caps */
void txt_color(UINT8 x, UINT8 y, UINT8 w, UINT8 h, UINT8 pal);   /* recolour a rectangle with BG palette 0..7 (0..3 are the text colours) */
void txt_pal_reset(void);                             /* reload the four text palettes (after a screen borrowed them) */
/* a pulsing menu arrow: call every frame with a running frame counter f; redraws the arrow cell every 16 frames */
#define UI_BLINK(x,y,f) do{ if(!((f)&15)) txt_line((x),(y),((f)&16)?S_CURSOR:">",1); }while(0)
#endif
