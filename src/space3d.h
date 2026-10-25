#ifndef SPACE3D_H
#define SPACE3D_H
#include <gb/gb.h>
/* The 3D view of space (bank 18). Unlocked at a station once your fame reaches rank POOR; switch it on or off in the station's VIEW row.
   The same galaxy as the flat view (same stars, stations, jobs, market, pirates), seen from the cockpit: you turn and climb, the worlds
   are shaded balls that grow as you fly at them, pirates fly in 3D and shoot orbs at you. Same results as space_run: SP_LAND, SP_TITLE,
   or SP_FLIP (the view was switched at a station: space_run starts the other view where the ship is). keep = 1: carry on from where the
   ship is (shx, shy) instead of starting beside the home world. */
int space3d_run(UINT8 keep) BANKED;
/* The Arena (bank 19): a wave shooter in the 3D engine, unlocked at fame rank AVERAGE. Start it from EXTRAS (SYSTEM menu) while flying.
   It has its own intro, score and results, pays gold, keeps the best wave in the save. It ends with the screen off: the caller restarts the view (SP_FLIP). */
void arena_run(void) BANKED;
/* The EXTRAS page: what is unlocked, with its switches. in_space = 1 when flying (the view can switch, the Arena can start).
   Returns 0 (nothing), 1 (the view was switched), 2 (start the Arena). The LCD is off when it is called; it turns it on. */
UINT8 extras_run(UINT8 in_space) BANKED;
#endif
