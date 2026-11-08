#pragma bank 38
/* BUGSTAR GBC - the save guard: keeps bank 0 small by living in its own bank (called from slot_read). */
#include <gb/gb.h>
#include "save.h"
#include "hud.h"
#include <string.h>
#include "bug.h"
#include "bugspr.h"

/* A save that passed its checksum can still hold odd numbers (an old build, a flipped bit that kept the sum, a hand-edited file). Pull every field the game uses as an index or a loop bound into range. */
void sv_sanitize(Save *s) BANKED {
    UINT8 i;
    if(s->level < 1) s->level = 1;
    if(s->level > 999) s->level = 999;
    if(s->zone > 99) s->zone = 99;
    if(s->hp > hud_hpmax(s->level)) s->hp = hud_hpmax(s->level);
    if(s->mp > hud_mpmax(s->level)) s->mp = hud_mpmax(s->level);
    if(s->cores > 8) s->cores = 8;
    if(s->potions > 9) s->potions = 9;
    s->name[8] = 0;
    for(i = 0; i < 16; i++) if(s->up[i] > 10) s->up[i] = 10;
    for(i = 0; i < 8; i++) if(s->el.cargo[i] > 99) s->el.cargo[i] = 99;
    if(s->el.fuel > tank_tab[5]) s->el.fuel = tank_tab[5];
    if(s->gift > 15) s->gift = 0;
    if(s->quirk > 15) s->quirk = 0;
    bug_clamp(&s->look);
}

/* Older saves are not converted any more: version 6 changed what a world is (twelve kinds, 32 weird traits) and what a save holds, so a slot written by
   an older game is erased the first time it is looked at, and sv_wiped says which slots, so the title can tell the player (once). */
UINT8 sv_wiped;
UINT8 sv_old(UINT8 slot, Save *out) BANKED {
    UINT8 z[8];
    (void)out;
    sram_read_at(slot, 0, z, 6);                                         /* the real copy: magic (4 bytes), version (2) */
    if(z[0] == (SAVE_MAGIC & 255) && z[1] == ((SAVE_MAGIC >> 8) & 255) && z[2] == ((SAVE_MAGIC >> 16) & 255) && z[3] == ((SAVE_MAGIC >> 24) & 255) && (z[4] != SAVE_VER || z[5] != 0)){
        memset(z, 0, 8); sram_write_at(slot, 0, z, 8); sram_write_at(slot, 4096, z, 8); sv_wiped |= 1 << slot;
    }
    return 0;
}
