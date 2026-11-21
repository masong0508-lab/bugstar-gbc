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
#endif
