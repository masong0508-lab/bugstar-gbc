#ifndef DUNGEON_EXTRA_H
#define DUNGEON_EXTRA_H
#include <gb/gb.h>
#include "dungeon_art.h"
/* Eight additional two-frame 8x8 enemy silhouettes, two tiles per frame.
   Each has the same sprite cost as the original four monsters. */
#define DG_EXTRA_KINDS 8
#define DG_EXTRA_TILES (DG_EXTRA_KINDS * 4)
#define DG_EXTRA_TILE(kind, frame) (MON_NTILES + (((kind) - 4) * 4) + ((frame) * 2))
void dg_extra_load(UINT8 mon0) BANKED;
/* Called only for the eight new kinds; returns steps this frame (0..4).
   Optional lateral steering gives moths, shades and crabs recognizable
   patterns. The dungeon still checks every move against solid walls. */
#include "dungeon_int.h"
UINT8 dg_extra_motion(Mon *m, INT16 *dx, INT16 *dy, UINT8 step) BANKED;
void kill_pay(Mon *m) BANKED;
UINT8 dg_pit_kind(UINT8 wave, UINT8 slot, UINT8 roll) BANKED;
#endif
