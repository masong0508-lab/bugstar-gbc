#ifndef KART_H
#define KART_H
#include <gb/gb.h>
/* KART WORLD (bank 24): a 3D kart race in EXTRAS. 3 laps, 3 rivals. The road is a picture in perspective whose lines slide sideways
   (an HBlank interrupt, src/raster.c) to make the bends; karts and trees are sprites that grow as they come near.
   Pays gold for the place you finish in; the best lap is kept in ex.rsv (EX_KART in spell.h). The LCD is off on entry and exit. */
#define NRIV 3
typedef struct { UINT32 seed; UINT16 tt[3]; UINT8 cup, chk; } KSave;     /* kept after the Extra block of the slot (SRAM offset 400): best trial times, the best cup */
#define KS_OFF 400U
extern KSave ks;
extern UINT8 kart_rk[1 + NRIV];                       /* how each racer finished the last race (1 is first; 0 is you) */
void kart_run(void) BANKED;                           /* (bank 25) the menus: GRAND PRIX, TIME TRIAL, QUICK RACE */
UINT8 kart_race(UINT8 t, UINT8 tt, UINT8 gpn) BANKED; /* (bank 24) one race: track 0..2, tt 1 = alone, gpn 1..3 inside a grand prix. Returns your place, 0 if you quit */
extern UINT16 tot_t, best_run;                        /* this race's time and best lap, in 60ths of a second */
extern UINT8 rivals_on;
void kart_results(UINT8 place, UINT8 saved, UINT8 gpn) BANKED;   /* (bank 25) the box after a race; pays the prize */
extern UINT8 air_t, air_trick;                        /* the stunt jump: frames in the air (0 = on the ground), and whether you did a trick */
UINT8 kart_air(UINT16 pm, INT16 lx, INT16 v, UINT8 b) BANKED;   /* (bank 25) once a frame: starts a jump off a pad, flies, tricks with B; returns 1 on a landing after a trick (boost!) */
INT16 kart_pad(UINT16 pm, INT16 *lat) BANKED;         /* (bank 25) the next jump pad ahead: its distance (999 = none) and lane */
void kart_air_reset(void) BANKED;
#define air_h() (air_t ? (UINT8)(((UINT16)air_t * (UINT8)(44 - air_t)) >> 3) : 0)   /* how high you are, in pixels (AIRN is 44) */
void kart_setup(void) BANKED;
void kart_pal0(void) BANKED;
void kart_ks_save(void) BANKED;
#endif
