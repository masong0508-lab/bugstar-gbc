#ifndef ELITE_H
#define ELITE_H
#include <gb/gb.h>
/* The Elite layer's screens: elite.c (bank 16: the station) and galaxy.c (bank 17: the galaxy chart). Space calls these. */
UINT8  elite_dock(UINT8 star) BANKED;                       /* returns 1 when the person switched the 2D / 3D view (the caller restarts space). The station of that star's world: market, service, shipyard, jobs, lounge. Hides sprites, restores them. */
#define GAL_NONE  0xFFFF
#define GAL_BEAST 300
/* the galaxy chart: pick a world to jump to. `from` is where the ship is (a star number). `beast` = 1 when the Technobeast can be jumped to.
   Returns the star, GAL_BEAST, or GAL_NONE (B). The fuel is already paid when it returns a star. The window is left for the caller to repaint. */
UINT16 galaxy_run(UINT8 from, UINT8 beast) BANKED;
#endif
