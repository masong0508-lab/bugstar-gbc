#ifndef GUIDE_H
#define GUIDE_H
#include <gb/gb.h>
/* The how-to-play guide (bank 22): 17 pages of plain words with four little animated pictures each (tools/make_guide.py).
   It borrows VRAM sprite tiles 128..223 and OAM 0..11, so a scene must reload its pictures afterwards (sys_gfx_lost). */
extern const UINT8  guide_tiles[96 * 16];
extern const UINT16 guide_pals[24 * 4];
void guide_run(void) BANKED;
#endif
