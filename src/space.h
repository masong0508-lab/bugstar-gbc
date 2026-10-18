#ifndef SPACE_H
#define SPACE_H
#include <gb/gb.h>
/* step 8: space and the ship (bank 6, with its art) */
#define SP_TITLE 1
#define SP_LAND  2
int space_run(void) BANKED;      /* fly from the world of sv.star; returns SP_LAND (sv.star = where you came down) or SP_TITLE */
#endif
