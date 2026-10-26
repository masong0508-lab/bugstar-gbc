#ifndef FLEET_H
#define FLEET_H
#include <gb/gb.h>
#include <gb/cgb.h>
/* FLEET (banks 32, 33): the SHIP BUILDER (body, wings, guns, colours), WINGMEN that fly beside you in 2D space, DRONE OUTPOSTS you
   can take and collect from, and FLEET BATTLES (an arena fight with your wingmen). Reached from the station's dock menu (FLEET).
   Saved in its own block at SRAM offset 600 of the slot (own seed + sum, so a new game starts empty). */
typedef struct {
    UINT32 seed;
    UINT32 coll;             /* play seconds at the last collection of outpost income */
    UINT16 wins;             /* fleet battles won */
    UINT8  body, wing, gun, col, wm;   /* the ship's parts (all 0 = the original ship), wingmen hired 0..3 (copies of fh, for saving) */
    UINT8  out[32];          /* a bit per star: its drone outpost is yours */
    UINT8  chk;
} FSave;
#define FS_OFF 600U
typedef struct { UINT8 body, wing, gun, col, wm; } FHot;     /* the parts that space reads every frame: always in the always-there RAM */
extern FHot fh;
#define FL_SPEED() ((fh.body == 1 ? 6 : 0) + (fh.wing == 3 ? 3 : 0))     /* top speed in space, 1/16 px a frame */
#define FL_ARMOR() ((fh.body == 2 ? 2 : 0) + (fh.wing == 2 ? 1 : 0))     /* damage soaked up in space */
#define FL_GUN()   (fh.body == 3 ? 1 : 0)                                  /* laser damage */


void  fleet_hq(UINT8 star) BANKED;                     /* (bank 32) the FLEET menu at a station */
void  fleet_ship_gfx(void) BANKED;                     /* (bank 32) space's ship tiles (VRAM bank 1) and colours as built; LCD off */
void  fleet_wing(UINT8 frame, UINT16 sx, UINT16 sy, UINT8 dir, UINT16 cx, UINT16 cy) BANKED;   /* (bank 32) wingmen: move, shoot, draw (2D space) */
void  fleet_wing_hide(void) BANKED;
UINT8 fleet_battle(UINT8 tier, UINT8 boss, UINT8 *kills) BANKED;   /* (bank 33) 1 won, 0 lost or retreated */
#endif
