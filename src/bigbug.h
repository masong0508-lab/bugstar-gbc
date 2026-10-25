#ifndef BIGBUG_H
#define BIGBUG_H
#include <gb/gb.h>
#include "bug.h"
/* The big preview in the bug creator: the bug at double size (smoothed with the EPX rule) drawn with background tiles,
   12 x 10 tiles at map (BIG_X, BIG_Y). Tiles live in VRAM bank 1 (set 0 = tiles 0..119, set 1 = 120..239: the two walking poses).
   The pixels are turned into tiles a row at a time (big_step, once a frame), so the creator never stalls.
   Palettes: BG 4..7 are the bug's four palettes, BG 2 the one for tiles where the face meets a hat or horn; colour 0 of all of them is the stage. */
#define BIG_X 1
#define BIG_Y 2
#define BIG_TW 12
#define BIG_TH 10
void big_colors(const BugLook *l, UINT16 stage) BANKED;       /* (re)load the six palettes */
void big_start(const BugLook *l) BANKED;                      /* the look changed: redraw both poses (call bug_build first is not needed) */
UINT8 big_step(void) BANKED;                                  /* do a little of the redraw; 1 while there is more to do */
UINT8 big_ready(void) BANKED;                                 /* bit 0: pose 0 is drawn, bit 1: pose 1 is */
void big_show(UINT8 set, UINT8 flip) BANKED;                  /* put pose `set` on the screen (flip: face left) */
void big_clear(void) BANKED;                                  /* blank stage */
#endif
