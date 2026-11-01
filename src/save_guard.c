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

/* The old save formats, converted on load (moved out of save.c so bank 0 has room). Returns 1 when slot holds a good save of version 1-4, filled into *out as version 5. */
typedef struct {                 /* the version 1 slot (steps 3-4), converted on load */
    UINT32 magic; UINT16 ver; UINT32 seed; UINT8 zone; UINT16 level; UINT16 hp, mp; UINT32 gold, xp, secs; UINT16 saves; char name[9]; UINT8 rsv[20]; UINT32 sum;
} Save1;

typedef struct {                 /* the version 2 slot: the same, but the bug had 13 parts (the last three are none in a converted save) */
    UINT32 magic; UINT16 ver; UINT32 seed; UINT8 zone; UINT16 level; UINT16 hp, mp; UINT32 gold, xp, secs; UINT16 saves; char name[9];
    UINT16 col[BC_N]; UINT8 pt[13];
    UINT8 friends, deepest, potions, star, cores, story, freed[8]; UINT16 warp_best; UINT8 rsv[7]; UINT32 sum;
} Save2;

typedef struct {                 /* the version 4 slot: as now, without the Elite layer (el) before the sum */
    UINT32 magic; UINT16 ver; UINT32 seed; UINT8 zone; UINT16 level; UINT16 hp, mp; UINT32 gold, xp, secs; UINT16 saves; char name[9];
    BugLook look;
    UINT8 friends, deepest, potions, star, cores, story, freed[8]; UINT16 warp_best; UINT8 rsv[4]; UINT8 up[16]; UINT32 sum;
} Save4;
typedef struct {                 /* the version 3 slot: as version 4, without the upgrades (up[]) */
    UINT32 magic; UINT16 ver; UINT32 seed; UINT8 zone; UINT16 level; UINT16 hp, mp; UINT32 gold, xp, secs; UINT16 saves; char name[9];
    BugLook look;
    UINT8 friends, deepest, potions, star, cores, story, freed[8]; UINT16 warp_best; UINT8 rsv[4]; UINT32 sum;
} Save3;

UINT8 sv_old(UINT8 slot, Save *out) BANKED {
    union { Save1 o; Save2 t; Save3 t3; Save4 t4; } u;   /* one buffer for every old format (four separate ones cost a 441-byte stack frame: a save overflowed the stack) */
    sram_read(slot, &u.t4, sizeof u.t4);                                 /* a version 4 save: no Elite layer yet (a full tank, an empty hold) */
    if(u.t4.magic == SAVE_MAGIC && u.t4.ver == 4 && u.t4.sum == fnv(&u.t4, sizeof u.t4 - 4)){
        memset(out, 0, sizeof *out);
        memcpy(out, &u.t4, sizeof u.t4 - 4); out->ver = SAVE_VER; memset(&out->el, 0, sizeof out->el); out->el.fuel = tank_tab[0];
        return 1;
    }
    sram_read(slot, &u.t3, sizeof u.t3);                                 /* a version 3 save: everything is the same, the upgrades start at zero */
    if(u.t3.magic == SAVE_MAGIC && u.t3.ver == 3 && u.t3.sum == fnv(&u.t3, sizeof u.t3 - 4)){
        memset(out, 0, sizeof *out);
        memcpy(out, &u.t3, sizeof u.t3 - 4); out->ver = SAVE_VER; memset(&out->el, 0, sizeof out->el); memset(out->up, 0, sizeof out->up); out->el.fuel = tank_tab[0];
        return 1;
    }
    sram_read(slot, &u.t, sizeof u.t);                                   /* a version 2 save: the same slot, a bug with 13 parts */
    if(u.t.magic == SAVE_MAGIC && u.t.ver == 2 && u.t.sum == fnv(&u.t, sizeof u.t - 4)){
        memset(out, 0, sizeof *out);
        out->magic = SAVE_MAGIC; out->ver = SAVE_VER; out->seed = u.t.seed; out->zone = u.t.zone; out->level = u.t.level; out->hp = u.t.hp; out->mp = u.t.mp;
        out->gold = u.t.gold; out->xp = u.t.xp; out->secs = u.t.secs; out->saves = u.t.saves; memcpy(out->name, u.t.name, 9);
        memcpy(out->look.col, u.t.col, sizeof u.t.col); memcpy(out->look.pt, u.t.pt, 13);
        out->friends = u.t.friends; out->deepest = u.t.deepest; out->potions = u.t.potions; out->star = u.t.star; out->cores = u.t.cores; out->story = u.t.story;
        memcpy(out->freed, u.t.freed, 8); out->warp_best = u.t.warp_best; out->el.fuel = tank_tab[0];
        return 1;
    }
    sram_read(slot, &u.o, sizeof u.o);                                   /* an old step 3-4 save: keep it, give the bug the CLASSIC look */
    if(u.o.magic != SAVE_MAGIC || u.o.ver != 1 || u.o.sum != fnv(&u.o, sizeof u.o - 4)) return 0;
    memset(out, 0, sizeof *out);
    out->magic = SAVE_MAGIC; out->ver = SAVE_VER; out->seed = u.o.seed; out->zone = u.o.zone; out->level = u.o.level; out->hp = u.o.hp; out->mp = u.o.mp;
    out->gold = u.o.gold; out->xp = u.o.xp; out->secs = u.o.secs; out->saves = u.o.saves; memcpy(out->name, u.o.name, 9);
    bug_default(&out->look); out->el.fuel = tank_tab[0];
    return 1;
}
