#ifndef PLANET_H
#define PLANET_H
#include <gb/gb.h>
/* step 6: the planet, top-down (bank 4, with its art) */
#define PL_TITLE  1
#define PL_LAUNCH 2
int planet_run(void) BANKED;     /* walk the world of star sv.star; returns PL_TITLE (quit to the title) or PL_LAUNCH (back into space) */
#endif
