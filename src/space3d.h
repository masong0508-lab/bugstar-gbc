#ifndef SPACE3D_H
#define SPACE3D_H
#include <gb/gb.h>
/* The 3D view of space (bank 18). Unlocked at a station once your fame reaches rank POOR; switch it on or off in the station's VIEW row.
   The same galaxy as the flat view (same stars, stations, jobs, market, pirates), seen from the cockpit: you turn and climb, the worlds
   are shaded balls that grow as you fly at them, pirates fly in 3D and shoot orbs at you. Same results as space_run: SP_LAND, SP_TITLE,
   or SP_FLIP (the view was switched at a station: space_run starts the other view where the ship is). keep = 1: carry on from where the
   ship is (shx, shy) instead of starting beside the home world. */
int space3d_run(UINT8 keep) BANKED;
#endif
