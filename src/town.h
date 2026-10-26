#ifndef TOWN_H
#define TOWN_H
#include <gb/gb.h>
/* The cities of a planet (bank 13). planet.c builds them on the map (houses, stalls, walls, a gate); stepping on the gate calls town_run.
   A town is a hamlet (tier 0), village, town, city or metropolis (tier 4): the bigger, the more it has. Everything about it comes from the
   world's seed and its number (0..2): the name, the greeting, which special places it has. One quest at a time is kept in the save. */
UINT8 town_run(UINT8 star, UINT8 idx, UINT8 tier, INT8 ddx, INT8 ddy) BANKED;   /* ddx, ddy: where the door down is, in metatiles from the gate. Returns 0, or tier + 1 when the bug pit was paid for (the planet runs it: dungeon_pit) */
void  town_pit_reward(UINT8 res, UINT8 tier) BANKED;                          /* pay out what dungeon_pit returned */
void town_label(char *out, UINT8 star, UINT8 idx, UINT8 tier) BANKED;          /* "CITY BRAMWICK" (up to 20 characters) */
#endif
