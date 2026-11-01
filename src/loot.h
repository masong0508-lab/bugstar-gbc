#ifndef LOOT_H
#define LOOT_H
#include <gb/gb.h>
/* The treasure bag (bank 34): weapons, potions, food and oddities, found in chests and dropped by monsters and pirates, sold at shops (50%) and stations (60% + fame).
   Kept in its own SRAM block (offset 2048 of the slot bank), written at once on every change; no change to the save format. */
void  loot_run(UINT8 mode) BANKED;                      /* 0: the BAG (A uses / equips), 1: sell at a planet shop or city store (50%), 2: sell at a station (60% + 2% a fame rank) */
UINT8 loot_bite(void) BANKED;                           /* the bite bonus of the weapon in hand */
UINT8 loot_chest(UINT8 depth, char *msg) BANKED;        /* a chest's find: puts it in the bag and writes the dialogue line into msg; 0 when the chest had no loot */
void  loot_kill(UINT8 kind, UINT8 depth) BANKED;        /* a monster died: sometimes food or an oddity */
void  loot_odd(void) BANKED;                            /* a pirate was shot down (space): sometimes an oddity falls out */
UINT8 loot_armor(void) BANKED;                          /* hits taken less: armor + the WARD CHARM */
UINT16 loot_clover(UINT16 g) BANKED;                    /* extra gold with the CLOVER charm worn */
UINT8 loot_xp(UINT8 x) BANKED;                          /* extra XP with the XP BEAD worn */
void  loot_regen(void) BANKED;                          /* LOCKET: call every couple of seconds in the dungeon */
UINT8 loot_eat(char *msg) BANKED;                       /* dungeon hotkey: eat the smallest food that is not wasted; msg gets the line to show; 1 if something was eaten */
#endif
