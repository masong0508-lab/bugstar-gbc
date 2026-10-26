#ifndef DUNGEON_H
#define DUNGEON_H
#include <gb/gb.h>
/* step 7: the dungeon, top-down (bank 5, with its art) */
enum { DG_EXIT = 1, DG_DOWN, DG_FAINT, DG_WIN, DG_TITLE };
int dungeon_run(void) BANKED;    /* down the door of the world in play; returns how you came out: DG_EXIT, DG_FAINT, DG_WIN or DG_TITLE */
int dungeon_hell(void) BANKED;   /* EXTRAS: the HELL DEPTHS, an endless stair down. Returns DG_EXIT, DG_FAINT or DG_TITLE */
int dungeon_pit(UINT8 tier) BANKED;   /* the city pit (tier 3 or 4: harder): waves of monsters in one room. Returns (waves cleared << 4) | DG_WIN, DG_EXIT or DG_FAINT; no gold is lost */
#endif
