#ifndef SAVE_H
#define SAVE_H
#include <gb/gb.h>
#include "bug.h"

/* SRAM map (MBC5, 4 banks of 8 KB = 32 KB, see Makefile -Wm-ya4):
     bank 0,1,2 : save slot 1,2,3 (one whole bank each, so a slot can grow later)
     bank 3     : options + last-used slot at offset 0
   The GBA game's 64-bit economy and huge galaxy address are gone: a world is one 32-bit seed. */
#define NSLOT 3

typedef struct {                 /* one save slot: 96 bytes on the cartridge (version 3; versions 1 (64 bytes) and 2 (the bug had 13 parts) are converted on load) */
    UINT32 magic;                /* SAVE_MAGIC */
    UINT16 ver;
    UINT32 seed;                 /* the world: everything is generated from this */
    UINT8  zone;
    UINT16 level;
    UINT16 hp, mp;
    UINT32 gold, xp;
    UINT32 secs;                 /* play time */
    UINT16 saves;                /* how many times this slot was saved */
    char   name[9];              /* the bug's name (8 chars + NUL) */
    BugLook look;                /* the bug, from the creator (step 5) */
    UINT8  friends;              /* bug friends freed from cages (step 7) */
    UINT8  deepest;              /* the deepest dungeon floor reached */
    UINT8  potions;              /* health potions carried (SELECT drinks one) */
    UINT8  star;                 /* the star (world) you are on or last landed on, 0..255; 0 = the home world (step 8) */
    UINT8  cores;                /* Bolt's data cores won back, 0..8: one per world freed (step 10) */
    UINT8  story;                /* ST_* flags */
    UINT8  freed[8];             /* the stars whose worlds gave a core, in order */
    UINT16 warp_best;            /* the most gold earned in one warp (the rail shooter) */
    UINT8  rsv[4];              /* (new fields come out of here; a fresh game zeroes them, older saves had them zero too) */
    UINT32 sum;                  /* FNV-1a of everything before it */
} Save;

typedef struct {                 /* options, same set as the GBA game */
    UINT8 music;                 /* 0 off, 1 low, 2 full */
    UINT8 sfx, autosave, shake;
    UINT8 diff;                  /* 0 EASY .. 4 NIGHTMARE */
    UINT8 baby, hunger, faint;   /* hunger/faint: 1 = on (default) */
    UINT8 xpm, goldm;            /* 0 x1, 1 x2, 2 x5 */
    UINT8 cam, minimap, nums, hints, arrow;
} Opts;

#define ST_INTRO 1               /* Bolt has told the story so far */
#define ST_BEAST 2               /* ...and met the Technobeast */
#define ST_DONE  4               /* the Technobeast is beaten: the ending was seen */
extern Save  sv;                 /* the slot in play */
extern Opts  opt;
extern UINT8 cur_slot;           /* 0..2 */
extern UINT8 last_slot;          /* most recently saved slot */

void   save_init(void);          /* call once at boot: hooks the frame counter, loads options */
void   settings_save(void);
UINT8  slot_peek(UINT8 slot, Save *out);   /* 1 if the slot holds a good save (copy in *out) */
UINT8  slot_read(UINT8 slot);              /* load into sv; 1 if good */
void   slot_write(UINT8 slot);             /* save sv (play time and counter updated) */
void   slot_erase(UINT8 slot);
void   new_game(UINT8 slot, UINT32 seed, const BugLook *look, const char *name);  /* fresh game in that slot, saved straight away; 0 look/name = CLASSIC / "BUG" */
UINT8  first_good_slot(void);              /* last_slot if good, else the first good one, else 255 */
UINT32 frames32(void);                     /* VBL frames since boot */
UINT32 play_secs(void);
void   play_reset(UINT32 base);
UINT32 world_seed(UINT8 star);
UINT8  world_freed(UINT8 star);            /* 1 if that world's queen gave up a core already */             /* the seed of star n's world (star 0 is sv.seed itself, so older saves keep their world) */
#endif
