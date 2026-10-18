#ifndef TITLE_SCREEN_H
#define TITLE_SCREEN_H
#include <gb/gb.h>   /* (title.c and title_art.c live in bank 2) */
enum { TI_CONT, TI_NEW, TI_LOAD, TI_OPT };   /* the four baked-in menu lines, top to bottom */
extern unsigned int title_frames;            /* frames spent on the title: a free source of seed entropy */
void title_init(void) BANKED;
/* have_save: 0 = CONTINUE and LOAD GAME are skipped by the cursor. Returns TI_*. */
int title_run(unsigned char have_save) BANKED;
/* hand the screen back to the printf text mode (font, palette, sprites) */
void title_exit(void) BANKED;
#endif
