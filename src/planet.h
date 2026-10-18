#ifndef PLANET_H
#define PLANET_H
#include <gb/gb.h>
/* step 6: the planet, top-down (bank 4, with its art) */
#define PL_TITLE 1
int planet_run(void) BANKED;     /* walk the planet of the slot in play (sv); returns PL_TITLE when the person quits to the title */
#endif
