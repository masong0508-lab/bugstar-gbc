#ifndef SPACE_INT_H
#define SPACE_INT_H
/* Space, inside: what space.c (bank 6: the map, the ship, landing, the warp) and space_foe.c (bank 10: pirates, orbs, debris,
   the Technobeast) share. Split so each fits its 16 KB bank with room to grow, and builds quicker. */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "wram.h"
#include "space_art.h"

#define SPR0      64             /* sprite tiles in VRAM bank 1 (the starfield uses bank-1 tiles 0..31) */
#define OAM_SHIP  20             /* 2 sprites */
#define OAM_LAS   22             /* 4 lasers x 2 */
#define OAM_PIR   30             /* 3 pirates x 2 */
#define OAM_ARR   36             /* the arrow, 2 */
#define OAM_CORE  0              /* the beast's core, 2 sprites: drawn over its body */
#define OAM_BODY  2              /* the beast's body, 18 sprites */
#define OAM_ORB   38             /* 2 orbs, one 8x16 sprite each */
#define NLAS      4
#define NPIR      3
#define NORB      6              /* orbs: 2 in sprites 38, 39; 4 more in 4..7 when the beast is not about */
#define ORB_OAM(i) ((i) < 2 ? OAM_ORB + (i) : 2 + (i))
#define NSPARK    4              /* debris from a kill, sprites 8..11 (when the beast is not about) */
#define SHIP_SX   80             /* the ship's middle on the screen */
#define SHIP_SY   64
#define BEAST_X   2048           /* the Technobeast's lair, where four cells meet: halfway across space from home, both ways */
#define BEAST_Y   2048
#define BEAST_HP  36

#define star_j WRAM_AT(WRAM_STARS, UINT8)          /* where each star's planet sits in its cell: x | y << 4 (metatiles, 2..12) */
#define star_k WRAM_AT(WRAM_STARS + 256, UINT8)    /* and its kind */

typedef struct { UINT16 x, y; INT8 dx, dy; UINT8 life; } Laser;

/* shared state (space.c owns it) */
extern UINT16 shx, shy;          /* the ship, 1/16 px (65536 = 4096 px: it wraps by itself) */
extern INT16  vx, vy;
extern UINT16 cam_x, cam_y;      /* top-left of the screen, world px */
extern UINT8  inv;               /* frames the ship can not be hurt */
extern UINT8  shown;             /* the HUD line: 255 HUD, 253 = redraw it */
extern UINT16 rng;
extern UINT8  b_on, b_dead, b_fight;   /* the Technobeast: here at all, dying (frames left), fighting you */
extern UINT16 b_x, b_y;          /* its middle, world px */

/* small helpers both halves use */
static INT16 wrapd(UINT16 a, UINT16 b){ INT16 d = (INT16)((a - b) & 4095); return d > 2048 ? d - 4096 : d; }   /* a - b, the short way round */
static UINT8 rnd(void){ rng ^= rng << 7; rng ^= rng >> 9; rng ^= rng << 8; return (UINT8)rng; }
static UINT16 star_px(UINT8 s){ return ((UINT16)(s & 15) << 8) + ((star_j[s] & 15) << 4) + 16; }   /* a planet's middle, world px */
static UINT16 star_py(UINT8 s){ return ((UINT16)(s >> 4) << 8) + ((star_j[s] >> 4) << 4) + 16; }
static void spr16(UINT8 oam, UINT8 tile, INT16 sx, INT16 sy){  /* a 16x16 sprite with its middle at screen (sx, sy); off screen hides it */
    if(sx < -8 || sx > 168 || sy < -8 || sy > 136){ move_sprite(oam, 0, 0); move_sprite(oam + 1, 0, 0); return; }
    set_sprite_tile(oam, SPR0 + tile); set_sprite_tile(oam + 1, SPR0 + tile + 2);
    move_sprite(oam, (UINT8)sx, (UINT8)(sy + 8)); move_sprite(oam + 1, (UINT8)(sx + 8), (UINT8)(sy + 8));
}
static void spr8(UINT8 oam, UINT8 tile, INT16 sx, INT16 sy){   /* step 11d: a half-size (8x8) picture, absolute VRAM tile (SPH_...), middle at (sx, sy); the pair's second sprite stays hidden */
    move_sprite(oam + 1, 0, 0);
    if(sx < -8 || sx > 168 || sy < -8 || sy > 136){ move_sprite(oam, 0, 0); return; }
    set_sprite_tile(oam, tile); move_sprite(oam, (UINT8)(sx + 4), (UINT8)(sy + 12));
}
static void hide2(UINT8 oam){ move_sprite(oam, 0, 0); move_sprite(oam + 1, 0, 0); }

extern UINT8 toast_t;
void toast(const char *t1, const char *t2);   /* (space_foe.c, same bank as its callers) two lines over the HUD for a moment */

/* space_foe.c (bank 10) */
void  foes_reset(void) BANKED;                  /* a fresh flight: no pirates, orbs, debris; the beast if 8 cores and not beaten */
void  foes_clear(void) BANKED;                  /* after a warp: everything gone, the beast keeps its wounds */
void  foes_gfx(void) BANKED;                    /* sprite tiles and colours for the foes (LCD off) */
UINT8 foes_step(UINT8 frame) BANKED;            /* move everything; 1 when the beast has just died (play the ending) */
UINT8 foes_target(UINT16 x, UINT16 y, INT16 *tx, INT16 *ty) BANKED;   /* the nearest pirate in range: its offset (for the wingmen) */
UINT8 foes_hit(Laser *l) BANKED;                /* a laser against the foes: 1 if it hit something (the laser is used up) */
void  foes_draw(UINT8 frame) BANKED;            /* sprites (call in VBlank) */
#endif
