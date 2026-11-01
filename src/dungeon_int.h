/* BUGSTAR GBC - the dungeon's private shared state (dungeon.c in bank 5 and dungeon_gen.c in bank 37 both use it). Not for other files. */
#ifndef DUNGEON_INT_H
#define DUNGEON_INT_H
#include "wram.h"
#include "dungeon_art.h"
#define rooms dg_rooms
#define nrooms dg_nrooms
#define mon dg_mon
#define depth dg_depth
#define floors dg_floors
#define kind dg_kind
#define hell dg_hell
#define pit dg_pit
#define evt dg_evt
#define lab dg_lab
#define rng dg_rng
#define bx dg_bx
#define by dg_by
#define dn_x dg_dn_x
#define dn_y dg_dn_y
#define DW        32               /* the floor is DW x DW metatiles */
#define NROOM     8
#define NMON      10
#define VIEW_W    11
#define VIEW_H    9
#define BUG_SX    80               /* the screen spot the camera keeps the bug's feet near */
#define BUG_SY    72
#define MON_TILE0 64               /* monster sprite tiles live in VRAM bank 1 from here */
#define OAM_BITE  0                /* 0,1: the bite flash. 4..14: the bug. 16..39: monsters */
#define OAM_MON0  16

enum { K_SPIDER, K_BEETLE, K_WASP, K_SLIME, K_QUEEN };
typedef struct { UINT16 x, y; UINT8 kind, hp, cool, flash, frame, alive; INT8 kx, ky; UINT8 st, t; INT8 lx, ly; } Mon;
/* st/t: spiders crouch (st 1, they blink) then lunge (st 2); a slime with st 9 is a small one that will not split again;
   the queen counts t up to calling her spiderlings */
typedef struct { UINT8 x, y, w, h; } Room;

#define dmap WRAM_AT(WRAM_DMAP, UINT8)                  /* DW x DW (wram.h) */
#define DM(x, y) dmap[((y) & 31) * DW + ((x) & 31)]
extern Room dg_rooms[NROOM]; extern UINT8 dg_nrooms; extern Mon dg_mon[NMON];
extern UINT8 dg_depth, dg_floors, dg_kind, dg_hell, dg_pit, dg_evt, dg_lab, dg_dn_x, dg_dn_y;
extern UINT16 dg_bx, dg_by, dg_rng;
void dg_make_floor(void) BANKED;                              /* dungeon_gen.c (bank 37): grow this floor from the seed */
#endif
