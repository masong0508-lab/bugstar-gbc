#ifndef SPACE_H
#define SPACE_H
#include <gb/gb.h>
/* step 8: space and the ship (bank 6, with its art) */
#define SP_TITLE 1
#define SP_LAND  2
#define SP_FLIP  4               /* (inside) the 2D / 3D view was switched at a station */
#define SP_END   3               /* the Technobeast is beaten: play the ending */
int space_run(void) BANKED;      /* fly from the world of sv.star; returns SP_LAND (sv.star = where you came down), SP_TITLE or SP_END */
void space_tables(void) BANKED;           /* fill star_j / star_k for sv.seed (the 3D view uses them too) */
void space3_pals(void) BANKED;            /* the space BG palette 3 and sprite palettes 0..5 (for the 3D view) */
void space_kind_pal(UINT8 slot, UINT8 kind) BANKED;   /* sprite palette `slot` = the colours of planets of that kind */
void title_pal_load(UINT8 pal) BANKED;    /* the title borrows the ship sprite: its colours (tiles: title_ship_load in space_art.h) */
void space_pals_copy(UINT8 dst, UINT8 src, UINT8 n) BANKED;   /* sprite palettes src.. of space into dst.. (for the warp) */
#endif
