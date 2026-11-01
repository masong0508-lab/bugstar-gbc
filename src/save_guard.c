#pragma bank 38
/* BUGSTAR GBC - the save guard: keeps bank 0 small by living in its own bank (called from slot_read). */
#include <gb/gb.h>
#include "save.h"
#include "hud.h"
#include "bug.h"

/* A save that passed its checksum can still hold odd numbers (an old build, a flipped bit that kept the sum, a hand-edited file). Pull every field the game uses as an index or a loop bound into range. */
void sv_sanitize(Save *s) BANKED {
    UINT8 i;
    if(s->level < 1) s->level = 1;
    if(s->level > 99) s->level = 99;
    if(s->zone > 99) s->zone = 99;
    if(s->hp > hud_hpmax(s->level)) s->hp = hud_hpmax(s->level);
    if(s->mp > hud_mpmax(s->level)) s->mp = hud_mpmax(s->level);
    if(s->cores > 8) s->cores = 8;
    if(s->potions > 9) s->potions = 9;
    s->name[8] = 0;
    for(i = 0; i < 16; i++) if(s->up[i] > 10) s->up[i] = 10;
    for(i = 0; i < 8; i++) if(s->el.cargo[i] > 99) s->el.cargo[i] = 99;
    if(s->el.fuel > tank_tab[5]) s->el.fuel = tank_tab[5];
    bug_clamp(&s->look);
}
