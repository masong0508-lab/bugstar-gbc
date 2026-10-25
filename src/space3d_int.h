#ifndef SPACE3D_INT_H
#define SPACE3D_INT_H
/* The 3D view, inside: space3d.c (bank 18: the maths, the sprite list, the art) and flight3d.c (bank 19: the flight, pirates, arena)
   share these. Variables are defined in space3d.c. */
#include <gb/gb.h>
#include "space_int.h"
#include "space3d_art.h"
#define S0       16
#define TS(n)    (S0 + S3_T_##n)
#define T_PIR    (S0 + S3_NTILES)           /* pirate, 2 frames of 4 tiles */
#define T_BOOM   (T_PIR + 8)                /* explosion, 2 frames */
#define T_ORB    (T_BOOM + 8)               /* an orb, 8x16 with the ball on top */
#define T_POD    (T_ORB + 2)                /* a crate */
#define PAL_MISC 0                          /* sprite palettes: 0 the ship's (station, crosshair, arrows, dust), 1 pirates, 2 lasers + crates, 3 orbs + blasts, 4..7 planets */
#define PAL_PIR  1
#define PAL_LAS  2
#define PAL_ORB  3
#define HUD3_Y   120                        /* the window: 2 HUD rows + 1 line of ours */

#define NP3      3
#define NB3      4
#define NO3      4
#define NPOD3    2
#define NDUST    5
#define LAND_R   46                         /* units: 'A' lands / docks when this close */
#define DOCK_R   40
#define ST_DX    100                        /* the station sits this far east of its planet */
#define PAL_BOSS 4                          /* the arena flagship's paint (planet palettes are free there) */
#define NPOD3x   NPOD3
typedef struct { UINT16 x, y; INT16 z; UINT8 hp, st, t, kind; } P3;          /* a pirate; st 0 none, 1 flying, 2.. blowing up */
typedef struct { UINT16 x, y; INT16 z; INT16 dx, dy, dz; UINT8 life; } B3;   /* a shot or an orb */
typedef struct { UINT16 x, y; INT16 z; UINT8 life, g; } Pod3;
typedef struct { UINT8 s, st; INT16 sx, sy, cz; } Cand;      /* st 0 planet, 1 station */
extern INT16  sz; extern UINT16 yaw; extern INT16 pit16, vel;
extern INT16  vfx, vfy, vfz, vrx, vry, vux, vuy, vuz;
extern P3     pir[NP3]; extern B3 bolt[NB3], orb[NO3]; extern Pod3 pod[NPOD3];
extern INT8   dust_x[NDUST], dust_y[NDUST]; extern UINT8 dust_z[NDUST];
extern UINT8  dn, prev_dn;
extern UINT8  pk3[4];
extern UINT8  near_pl, near_st, tgt; extern INT16 tgt_d;
extern UINT8  arena_on, edge_on; extern INT16 ed_x, ed_y, ed_z;
static INT16 unit(UINT16 a, UINT16 b){ return (INT16)(a - b) >> 4; }       /* a - b, 1/16 units -> units, the short way round */
static INT16 aabs(INT16 v){ return v < 0 ? -v : v; }
static UINT8 inbox(INT16 v){ return v > -500 && v < 500; }
static INT16 star_z(UINT8 s){ return (INT16)((star_j[s] * 7 + s * 13) & 127) - 64; }      /* a world's height, units */
/* bank 18 (space3d.c) */
void s3_dust_born(UINT8 i, UINT8 far) BANKED;   /* a new speck of dust */
void s3_basis(void) BANKED;                 /* the ship's three axes from yaw and pitch */
void s3_scan(void) BANKED;                  /* the worlds near the ship: near_pl, near_st, tgt */
void s3_render(UINT8 frame) BANKED;         /* the sprite list: cross, marks, things, worlds nearest first */
void s3_flush(UINT8 frame) BANKED;          /* (in VBlank) write the list, turn the star background */
void s3_load(void) BANKED;                  /* tiles, palettes, the star background (LCD off) */
#endif
