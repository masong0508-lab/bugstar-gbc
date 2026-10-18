#ifndef SAVE_H
#define SAVE_H
#include <gb/gb.h>
#include "bug.h"

/* SRAM map (MBC5, 4 banks of 8 KB = 32 KB, see Makefile -Wm-ya4):
     bank 0,1,2 : save slot 1,2,3 (one whole bank each, so a slot can grow later)
     bank 3     : options + last-used slot at offset 0
   The GBA game's 64-bit economy and huge galaxy address are gone: a world is one 32-bit seed. */
#define NSLOT 3

typedef struct {                 /* one save slot: 96 bytes on the cartridge (version 2; version 1 was 64 and is converted on load) */
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
    UINT8  rsv[20];              /* (new fields come out of here; a fresh game zeroes them, older saves had them zero too) */
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
#endif
