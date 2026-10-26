#ifndef WILD_H
#define WILD_H
#include <gb/gb.h>
#include <gb/cgb.h>
/* WILD: what lives on the worlds. Four small modules, one saved block (SRAM offset 480 of the slot, its own seed + sum, so a new game starts empty).
   bank 27 (wild.c)  : the BESTIARY: 4 creatures + a queen for each of the 6 kinds of world: names, colours, strength, kill counts, a book to read.
   bank 28 (weird.c) : WEIRD WORLDS: about 1 star in 9 has a trait (16 kinds x 9 strengths): a palette trick on the planet, and a small mark on you.
   bank 29 (life.c)  : LIVING CREATURES: harmless animals that wander the planet: shy ones run, friendly ones like a pat (A), curious ones follow you.
   bank 30 (town2.c) : TOWN EXTRAS: library, tailor, bank, arcade for the cities.
   Nothing here touches the Save or Extra blocks. */

typedef struct {
    UINT32 seed;
    UINT8  kills[30];             /* per creature: world kind * 4 + sprite (0 spider 1 beetle 2 wasp 3 slime), then 24 + world kind = that world's queen */
    UINT16 wseen;                 /* a bit per weird trait met on a planet */
    UINT16 bank;                  /* gold kept in the town bank */
    UINT8  fed, petn;             /* creatures caught or patted */
    UINT8  visits;                /* bank interest: visits since the last payout (counted in 5-minute steps of play) */
    UINT8  lib;                   /* the library's lore page, next */
    UINT8  arc;                   /* the arcade's best streak */
    UINT8  chk;
} XA;
#define XA_OFF 480U
/* The state lives in WRAM bank 2 (D000..; the stack has no room left in C000..CFFF). Every function here switches to bank 2 on entry and back to
   bank 1 before it returns: never hand one of them a pointer into D000..DFFF, and never call them from an interrupt. */
#include "wram.h"
#define XAP ((XA *)0xD000)
void wild_load(void) BANKED;                    /* make sure xa belongs to this slot (reads the SRAM block when the slot changed) */
void wild_save(void) BANKED;                    /* write it back */

/* ---- bank 27: the bestiary */
void  bst_pals(UINT8 wk) BANKED;                /* the four monster palettes (sprite palettes 0..3) in this world's colours */
UINT8 bst_hp(UINT8 hp, UINT8 k, UINT8 wk) BANKED;      /* a monster's health, shaped by what it is and by the world's weird trait */
UINT8 bst_dmg(UINT8 k, UINT8 wk) BANKED;               /* extra damage it does to you */
void  bst_kill(UINT8 k, UINT8 wk, UINT8 depth) BANKED; /* it died: count it, pay the weird world's gold bonus */
void  bst_name(char *out, UINT8 wk, UINT8 k) BANKED;   /* "SCARAB" (k 4 = the queen, 0..3 the sprites) */
void  bst_screen(void) BANKED;                         /* the book: one page for each world, then the weird worlds met */

/* ---- bank 28: weird worlds */
UINT8 weird_of(UINT8 star) BANKED;              /* 0 = a normal world, else (strength << 4) | trait, strength 1..9, trait 0..15 */
void  weird_name(char *out, UINT8 w) BANKED;    /* "WILD RAINBOW" */
void  weird_note(char *out, UINT8 w) BANKED;    /* what it does, up to 20 characters ("+12% GOLD") */
void  weird_pal(palette_color_t *p, UINT8 n, UINT8 w, UINT8 tick) BANKED;   /* the planet's colours, bent */
UINT8 weird_tick(UINT8 w, UINT8 tick) BANKED;   /* once a frame on the planet: healing or bad air. 1 if your health moved */
void  weird_land(UINT8 w) BANKED;               /* first landing: note it in the book, say it with Bolt */
UINT8 weird_pct(UINT8 w, UINT8 what) BANKED;    /* WP_*: how many percent of something this world adds (WP_HIT: whole damage points) */
#define WP_GOLD 0     /* gold from kills */
#define WP_FOE  1     /* monster health */
#define WP_HIT  2     /* monster damage points */
#define WP_ORE  3     /* mining pay */
void  weird_screen(UINT8 y0) BANKED;            /* the weird worlds found, as lines from row y0 */

/* ---- bank 29: living creatures */
void  life_gfx(UINT8 wk) BANKED;                          /* the tiles (sprite tiles 128..135, VRAM bank 1) and colours (sprite palette 3) of this world's animals */
void  life_load(UINT8 wk, UINT16 px, UINT16 py) BANKED;   /* four animals appear around (px, py): a clear spot, the ship's clearing */
void  life_step(UINT16 bx, UINT16 by, UINT8 tick) BANKED; /* think and move: every other frame */
UINT8 life_peek(UINT8 i, UINT16 *x, UINT16 *y) BANKED;    /* animal i (0..3): 1 and its feet if it moved this step: the planet checks the ground */
void  life_back(UINT8 i) BANKED;                          /* ... and sends it back if that was a wall or water */
void  life_draw(UINT16 cx, UINT16 cy) BANKED;             /* the sprites (OAM 15..18) */
UINT8 life_pat(UINT16 bx, UINT16 by) BANKED;              /* A near one: 0 nothing near, 1 patted (paid), 2 nuzzled (paid), 3 shy: ran off, 4 patted already */

/* ---- bank 30: town extras. Place ids start at 16 (town.c has 0..9). */
UINT8 town2_add(UINT8 *ids, UINT8 n, UINT8 tier, UINT16 th) BANKED;   /* append this town's extra places; returns the new n */
void  town2_name(UINT8 id, char *out) BANKED;
UINT8 town2_open(UINT8 id, UINT8 night) BANKED;                       /* 1 if the place is open now */
void  town2_go(UINT8 id, UINT8 tier, UINT16 th) BANKED;
#define SV2_LIBRARY 16
#define SV2_TAILOR  17
#define SV2_BANK    18
#define SV2_ARCADE  19
#endif
