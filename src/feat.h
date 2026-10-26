#ifndef FEAT_H
#define FEAT_H
#include <gb/gb.h>
#include <gb/cgb.h>
/* The extras (bank 20): the clock (day and night), pets, fishing, the tricks the queens give. Nothing here draws a scene: the planet,
   the dungeon and the towns call it. */
#define PET_TILE 200                                   /* sprite tiles (VRAM bank 1): 4 of them, 2 frames of one 8x16 sprite */
extern const UINT8 pet_tiles[64];

/* the clock: a day is 480 seconds of play (hours of 20 s). Sleeping at an inn moves it on to the morning. */
UINT8 tod_dark(void) BANKED;                          /* 0 (day) .. 16 (night), changing by the second */
UINT8 tod_hour(void) BANKED;                          /* 0..23 */
void  tod_tint(palette_color_t *p, UINT8 n) BANKED;    /* darken and cool n colours by the time of day */
void  tod_sleep(void) BANKED;                         /* sleep until morning */
void  tod_name(char *out) BANKED;                     /* "DAWN", "DAY", "DUSK", "NIGHT" */

/* pets: your freed friends (up to 3) trail behind you. `oam` = 3 sprite slots. wrap = 1 on the planet (the map wraps at 4096) */
void  pet_load(void) BANKED;                          /* sprite tiles into VRAM */
void  pet_reset(UINT16 x, UINT16 y) BANKED;
void  pet_push(UINT16 x, UINT16 y) BANKED;            /* call every frame with the bug's feet */
UINT8 pet_get(UINT8 i, UINT16 *x, UINT16 *y) BANKED;  /* where pet i is (0 if there is no pet i) */
void  pet_draw(const UINT8 *oam, UINT16 cx, UINT16 cy, UINT8 wrap) BANKED;

/* fishing: A at the water. Returns 1 when something was caught. Brings the HUD back when done. */
UINT8 fish_run(UINT8 world) BANKED;

/* the queen's trick: one for each kind of world (bits of ex.tricks) */
#define TRICK(k) (ex.tricks & (1 << (k)))
void  dungeon_won(UINT8 kind) BANKED;                 /* the words and the trick after a queen falls */
#endif
