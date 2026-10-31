#ifndef SHOP_H
#define SHOP_H
#include <gb/gb.h>
/* Upgrades (bank 15). ship_menu: the ship's FLY / SHOP / SKILLS / STAY box, returns 1 when the person chose to fly into space.
   skills_run: the SKILLS page (level-up points); it can run over any scene; the caller puts its own window (HUD, menu) back. skills_free: points not spent yet. */
UINT8 ship_menu(UINT8 start) BANKED;   /* start = tab * 4 + row; 0 closed, 1 fly, 2 FEATS page, 3 MINING (the caller runs those and re-opens on the CARE tab) */
void  skills_run(void) BANKED;
UINT8 skills_free(void) BANKED;
void  shop_run(UINT8 mode) BANKED;   /* 0: the planet shop (potions, bug gear); 1: a station's shipyard (ship parts, fame discount) */
void status_run(void) BANKED;                     /* the STATUS page: bars, gold, rank, gear, time (the pause menu's first page, like the GBA's BUG tab) */
#endif
