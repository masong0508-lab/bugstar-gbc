#ifndef BUG_H
#define BUG_H
#include <gb/gb.h>
/* The bug (step 5): a creature built from parts. Pure logic, no hardware: tests/host_test.c runs all of it on a PC.

   A BugLook is 13 part choices + 8 colours (29 bytes, saved with the slot). bug_build() turns it into sprites:
   the parts (art in bugparts.c, made by tools/make_bug.py) are stamped into a 2-bit picture, which is cut into
   8x16 sprite cells, each with one 4-colour palette. Geometry of the picture (facing right; x runs left to right):

        columns:   [tail] [shell x2..4] [head]        every column is 8 px wide
        shell:     wings  y  0..15   (palette 3)       <- sprite boxes of one column never overlap vertically,
                   shell  y 16..31   (palette 0)          so a scanline holds at most one sprite per column
        head:      hats   y  6..21   (palette 2)
                   face   y 22..37   (palette 1)

   A bug is at most 6 columns wide, so it never uses more than 6 of the 10 sprites a scanline allows (BUG_BUDGET).
   Each 8x16 cell has 3 colours + clear, so the 8 creator colours are shared out like this:
        palette 0  OUTLINE, SHELL, MARKINGS (or the shine when the pattern is plain)    legs and tail use these too
        palette 1  OUTLINE, HEAD, EYES                                                  head, eyes, mouth
        palette 2  OUTLINE, HORN, HAT                                                   antennae, horns, hats
        palette 3  WING shade, WING, WING light (both made from the one WING colour)    wings
   Frame 0/1 are the two walking poses (the legs swap; the sprites also bob one pixel). */

enum { BP_LENGTH, BP_SHAPE, BP_FINISH, BP_PAT, BP_EYE, BP_MOUTH, BP_ANT, BP_LEGS, BP_LEGST, BP_HORN, BP_WING, BP_TAIL, BP_HAT, BP_N };
enum { BC_SHELL, BC_MARK, BC_HEAD, BC_EYE, BC_HORN, BC_WING, BC_HAT, BC_LINE, BC_N };

typedef struct {
    UINT16 col[BC_N];            /* GBC 5-5-5 colours */
    UINT8  pt[BP_N];             /* part choices, each below bug_part_n[] */
} BugLook;

/* The look data lives in bug.c (bank 14), so other banks reach it through these calls (they copy strings into your buffer). */
UINT8  bug_part_cnt(UINT8 part) BANKED;                      /* how many choices that part has */
void   bug_part_label(UINT8 part, char *out) BANKED;         /* "LENGTH", "SHAPE" ... (8 characters at most) */
void   bug_part_name(UINT8 part, UINT8 choice, char *out) BANKED;   /* "NORMAL", "ROUND" ... (8 characters at most) */
void   bug_col_label(UINT8 c, char *out) BANKED;             /* "SHELL", "MARKINGS" ... */
UINT8  bug_col_on(const BugLook *l, UINT8 c) BANKED;         /* 0 when the part that colour paints is switched off */

#define BUG_NPRESET 18
void   bug_preset_name(UINT8 i, char *out) BANKED;
void   bug_preset(BugLook *l, UINT8 i, UINT16 lock_parts, UINT8 lock_cols) BANKED;   /* ready-made look i; a locked part (bit BP_x) or colour (bit BC_x) keeps what it has */
void   bug_default(BugLook *l) BANKED;                       /* the CLASSIC look */
void   bug_clamp(BugLook *l) BANKED;                         /* force every part into range (damaged save) */

/* the swatches, a colour scheme maker, the mixer's helpers */
#define BUG_NSWATCH 16
UINT16 bug_swatch(UINT8 i) BANKED;
void   bug_swatch_name(UINT8 i, char *out) BANKED;
UINT8  bug_swatch_of(UINT16 c) BANKED;                       /* which swatch has exactly that colour, or 255 */
#define BUG_NSCHEME 9
void   bug_scheme_name(UINT8 i, char *out) BANKED;
void   bug_scheme(BugLook *l, UINT8 anchor, UINT8 lock_cols, UINT8 scheme) BANKED;   /* every unlocked colour but `anchor` is made to go with it */
UINT16 bug_hsv(UINT16 h, UINT16 s, UINT16 v);                /* hue 0..359, saturation and value 0..255 -> 5-5-5 */
void   bug_rand_seed(UINT16 s) BANKED;
void   bug_random_name(char *out) BANKED;                    /* a bug-sounding name of 8 letters at most */
UINT8  bug_rand(UINT8 n) BANKED;                             /* 0..n-1 */
void   bug_surprise(BugLook *l, UINT16 lock_parts, UINT8 lock_cols) BANKED;

/* ---- sprites */
typedef struct { UINT8 w, h, ox, oy; const unsigned char *px; } BugPart;   /* 2 bits per pixel, 4 to a byte, row by row */
extern const BugPart bp_shell[], bp_wings[], bp_head[], bp_eye[], bp_mouth[], bp_ant[], bp_horn[], bp_hat[], bp_tail[];

#define BUG_FRAMES  2
#define BUG_MAXCELL 11               /* wing cells 0..3, shell cells 4..7, tail 8, hat 9, face 10 */
#define BUG_BW      48
#define BUG_BH      38
#define BUG_FEET    32               /* canvas row just under the feet */
#define BUG_BUDGET  6
typedef struct { UINT8 x, y, cell, pal; } BugSpr;            /* position inside the canvas (head to the right) */
typedef struct {
    UINT8  n;                                                /* sprites in use */
    UINT8  w;                                                /* canvas width in pixels (16 + the tail's 8 + head 8 + shell) */
    BugSpr s[BUG_MAXCELL];
    UINT8  tiles[BUG_FRAMES][BUG_MAXCELL][32];               /* one 8x16 sprite (two tiles) per cell and frame */
} BugSet;
extern BugSet bug_set;
void   bug_build(const BugLook *l) BANKED;                          /* draw both frames, cut them into bug_set */
void   bug_palettes(const BugLook *l, UINT16 *out16) BANKED;        /* 4 palettes x 4 colours (entry 0 of each is clear) */
void   bug_canvas(const BugLook *l, UINT8 f) BANKED;                /* frame f of the picture in the canvas, each pixel = region * 4 + colour (regions 0 shell, 1 face, 2 hat, 3 wings) */
UINT8  bug_hc(const BugLook *l) BANKED;                             /* the head's column (the bug is (hc + 1) * 8 pixels wide) */
UINT8  bug_peak(void);                                       /* the most sprites that any one scanline of the canvas holds */
UINT8  bug_pixel(UINT8 x, UINT8 y);                          /* the 2-bit picture of the last bug_build (frame 0), for tests and previews */
UINT8  bug_cell_pal(UINT8 cell);                             /* which palette cell c uses */
#endif
