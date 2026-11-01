#ifndef LOOT_H
#define LOOT_H
#include <gb/gb.h>
/* The treasure bag (bank 34): weapons, potions, food and oddities, found in chests and dropped by monsters and pirates, sold at shops (50%) and stations (60% + fame).
   Stored in ex.rsv[0..9] (the Extra block: no save format change; a bag of an older save is simply empty). */
void  loot_run(UINT8 mode) BANKED;                      /* 0: the BAG (A uses / equips), 1: sell at a planet shop or city store (50%), 2: sell at a station (60% + 2% a fame rank) */
UINT8 loot_bite(void) BANKED;                           /* the bite bonus of the weapon in hand */
UINT8 loot_chest(UINT8 depth, char *msg) BANKED;        /* a chest's find: puts it in the bag and writes the dialogue line into msg; 0 when the chest had no loot */
void  loot_kill(UINT8 kind, UINT8 depth) BANKED;        /* a monster died: sometimes food or an oddity */
void  loot_odd(void) BANKED;                            /* a pirate was shot down (space): sometimes an oddity falls out */
UINT8 loot_eat(char *msg) BANKED;                       /* dungeon hotkey: eat the smallest food that is not wasted; msg gets the line to show; 1 if something was eaten */
#endif
