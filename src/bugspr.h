#ifndef BUGSPR_H
#define BUGSPR_H
#include <gb/gb.h>
#include "bug.h"
/* Puts a bug on screen as sprites (8x16 mode). Sprite tiles 128.. (bank 0; the font owns 0..127), sprite palettes 4..7,
   OAM slots 4.. (the title cursor keeps slot 0). */
#define BUGSPR_TILE0 128
#define BUGSPR_PAL0  4
#define BUGSPR_OAM0  4
#define BUGSPR_UP    18        /* half-size bug (step 11): canvas top is this far above the spot the bug stands on (was 36 at full size) */
void bugspr_load(const BugLook *l);                 /* build + upload tiles and palettes (switches sprites to 8x16) */
void bugspr_colors(const BugLook *l);               /* palettes only: colour edits need nothing else */
void bugspr_show(UINT8 x, UINT8 y, UINT8 flip, UINT8 frame);   /* canvas top-left at screen x,y (the picture is 16 px high, half the old size); flip = face left */
UINT8 bugspr_w(void);                               /* canvas width in pixels */
void bugspr_hide(void);                             /* hide the bug and go back to 8x8 sprites */
#endif
