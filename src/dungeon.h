#ifndef DUNGEON_H
#define DUNGEON_H
#include <gb/gb.h>
/* step 7: the dungeon, top-down (bank 5, with its art) */
enum { DG_EXIT = 1, DG_DOWN, DG_FAINT, DG_WIN, DG_TITLE };
int dungeon_run(void) BANKED;    /* down the door of the world in play; returns how you came out: DG_EXIT, DG_FAINT, DG_WIN or DG_TITLE */
#endif
