#ifndef BUGMAKE_H
#define BUGMAKE_H
#include <gb/gb.h>
#include "bug.h"
/* The bug creator (step 5, remade): the bug at double size on a stage, a tab list (BODY, HEAD, GEAR, PAINT, ME) and four rows of the tab.
     UP/DOWN pick a row    LEFT/RIGHT change it (hold to keep going)    A: next choice, type the name, mix a colour, roll the dice
     SELECT: next tab      SELECT+A: lock a row (the dice and the ready-made looks skip it)
     B: undo (up to 6 steps), or leave without changes when there is nothing to undo     START: all done (it asks first)
   Edits *look and name; returns 1 when the person finished, 0 when they backed out (look and name untouched).
   first = 1 for a new game (the title says MAKE YOUR BUG), 0 for the mirror. Takes over the screen (VRAM bank 1 tiles 0..239, BG palettes 2 and 4..7). */
UINT8 bugmake_run(BugLook *look, char *name, UINT8 first) BANKED;   /* bank 3 */
#endif
