#ifndef BUGMAKE_H
#define BUGMAKE_H
#include <gb/gb.h>
#include "bug.h"
/* The bug creator (step 5): a live preview on top, a scrolling list of 13 parts, MATCH and 8 colours below.
     UP/DOWN pick a row    LEFT/RIGHT change it (hold on a colour to browse)    A: type the name, mix a colour, or next choice
     SELECT: surprise me   SELECT+A: lock a row (surprise and ready-made looks skip it)
     B: undo (up to 8 steps), or leave without changes when there is nothing to undo     START: all done
   Edits *look and name; returns 1 when the person finished, 0 when they backed out (look and name untouched).
   first = 1 for a new game (the title says MAKE YOUR BUG), 0 for the mirror. */
UINT8 bugmake_run(BugLook *look, char *name, UINT8 first) BANKED;   /* bank 3, with bug.c and its art */
#endif
