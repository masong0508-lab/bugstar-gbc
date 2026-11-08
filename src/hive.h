#ifndef HIVE_H
#define HIVE_H
#include <gb/gb.h>
/* BUG HIVE (bank 45): a small colony game in the EXTRAS. Build a farm, a mill, a quarry, houses, walls and towers; nectar, bark and rock come in as you play;
   raiders test the walls every few minutes; sell the stores for gold. Own SRAM block in the slot. The LCD is off on entry and exit. */
void   hive_run(void) BANKED;
UINT16 hive_info(char *out) BANKED;        /* "12 BUGS"; returns the bugs */
void   hive_reset(void) BANKED;
#endif
