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

extern const UINT8 bug_part_n[BP_N];                        /* how many choices each part has */
extern const char *const bug_part_label[BP_N];              /* "LENGTH", "SHAPE" ... (9 characters at most) */
extern const char *const *const bug_part_names[BP_N];       /* the names of the choices (8 characters at most) */
extern const char *const bug_col_label[BC_N];               /* "SHELL", "MARKINGS" ... */

#define BUG_NPRESET 18
extern const char *bug_preset_name(UINT8 i);
void   bug_preset(BugLook *l, UINT8 i, UINT16 lock_parts, UINT8 lock_cols);   /* ready-made look i; a locked part (bit BP_x) or colour (bit BC_x) keeps what it has */
void   bug_default(BugLook *l);                              /* the CLASSIC look */
void   bug_clamp(BugLook *l);                                /* force every part into range (damaged save) */
UINT8  bug_col_on(const BugLook *l, UINT8 c);                /* 0 when the part that colour paints is switched off */

/* 16 ready-made colours, a colour scheme maker, the mixer's helpers */
#define BUG_NSWATCH 16
extern const UINT16 bug_swatch[BUG_NSWATCH];
extern const char *const bug_swatch_name[BUG_NSWATCH];
#define BUG_NSCHEME 9
extern const char *const bug_scheme_name[BUG_NSCHEME];
void   bug_scheme(BugLook *l, UINT8 anchor, UINT8 lock_cols, UINT8 scheme);   /* every unlocked colour but `anchor` is made to go with it */
UINT16 bug_hsv(UINT16 h, UINT16 s, UINT16 v);                /* hue 0..359, saturation and value 0..255 -> 5-5-5 */
void   bug_rand_seed(UINT16 s);
UINT8  bug_rand(UINT8 n);                                    /* 0..n-1 */
void   bug_surprise(BugLook *l, UINT16 lock_parts, UINT8 lock_cols);

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
void   bug_build(const BugLook *l);                          /* draw both frames, cut them into bug_set */
void   bug_palettes(const BugLook *l, UINT16 *out16);        /* 4 palettes x 4 colours (entry 0 of each is clear) */
UINT8  bug_peak(void);                                       /* the most sprites that any one scanline of the canvas holds */
UINT8  bug_pixel(UINT8 x, UINT8 y);                          /* the 2-bit picture of the last bug_build (frame 0), for tests and previews */
UINT8  bug_cell_pal(UINT8 cell);                             /* which palette cell c uses */
#endif
