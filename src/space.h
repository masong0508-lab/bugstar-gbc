#ifndef SPACE_H
#define SPACE_H
#include <gb/gb.h>
/* step 8: space and the ship (bank 6, with its art) */
#define SP_TITLE 1
#define SP_LAND  2
#define SP_END   3               /* the Technobeast is beaten: play the ending */
int space_run(void) BANKED;      /* fly from the world of sv.star; returns SP_LAND (sv.star = where you came down), SP_TITLE or SP_END */
void title_pal_load(UINT8 pal) BANKED;    /* the title borrows the ship sprite: its colours (tiles: title_ship_load in space_art.h) */
#endif
