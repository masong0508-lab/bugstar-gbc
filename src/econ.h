#ifndef ECON_H
#define ECON_H
#include <gb/gb.h>
/* The Elite layer's numbers (bank 15): the eight goods and what they cost where, fame ranks, jobs. No hardware: tests/host_test.c runs it.
   A good's price depends on the kind of world it is sold on (what that world grows or lacks), a bit on the star itself, and
   drifts a little every few jumps. Stations buy for 12% less than they sell for, so a round trip with nothing in between never pays. */
enum { G_FOOD, G_ORE, G_CRYSTAL, G_SILK, G_AMBER, G_MEDS, G_TECH, G_NECTAR, G_N };
void   good_label(UINT8 g, char *out) BANKED;                      /* FOOD, ORE ... copied into out (the tables live in bank 15: other banks cannot read them) */
UINT16 econ_base(UINT8 g) BANKED;                                  /* the middle price of good g */
UINT16 econ_price(UINT8 star, UINT8 kind, UINT8 g) BANKED;         /* what the station sells it for (and what a pod of it is worth) */
UINT16 econ_sell(UINT8 star, UINT8 kind, UINT8 g) BANKED;          /* what the station pays for it */
UINT8  econ_load(void) BANKED;                                     /* units in the hold */
UINT8  econ_buy(UINT8 star, UINT8 kind, UINT8 g, UINT8 n) BANKED;     /* buy up to n units; returns how many */
UINT8  econ_sell_n(UINT8 star, UINT8 kind, UINT8 g, UINT8 n) BANKED;           /* sell up to n units; returns how many */

#define NRANK 9
void   rank_label(UINT8 r, char *out) BANKED;                      /* HARMLESS .. ELITE */
UINT8  fame_rank(void) BANKED;                                     /* 0..8 from sv.el.fame (8 = ELITE) */
UINT8  fame_disc(void) BANKED;                                     /* percent off at stations: 2 a rank */
void   fame_add(UINT16 n) BANKED;
#define RANK_3D    2                                              /* fame rank (POOR) that opens the 3D view */
#define RANK_ARENA 3                                              /* fame rank (AVERAGE) that opens the Arena */
UINT8  unlocks_update(void) BANKED;                               /* opens what the fame rank allows (bits in sv.el.rsv[0]: 1 the 3D view, 4 the Arena); returns the bits that opened just now */
UINT8  elite_kill(UINT8 kind) BANKED;                              /* a pirate shot down: kills, fame, the hunt job; 1 when that finished the job */

/* jobs */
enum { J_NONE, J_HUNT, J_SEND };
void   job_offer(UINT8 star, UINT8 *kind, UINT8 *n, UINT8 *dest, UINT16 *reward) BANKED;   /* the job this station has for you (same until you take or finish one) */
void   job_take(UINT8 kind, UINT8 n, UINT8 dest, UINT16 reward) BANKED;
UINT8  job_done(UINT8 here) BANKED;                                /* 1 if the job is finished and `here` is where it pays */
UINT16 job_pay(void) BANKED;                                       /* pay out and clear the job; returns the gold */
UINT8  cells_between(UINT8 a, UINT8 b) BANKED;                     /* the jump distance in cells (the bigger of the two ways round, 0..8) */
UINT8  jump_fuel(UINT8 a, UINT8 b) BANKED;                         /* the fuel a jump costs: 1 at least */
#endif
