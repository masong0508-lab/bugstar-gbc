/* BUGSTAR GBC - step 3: 3-slot battery save + options (port of the GBA save.inc, minus the 64-bit words) */
#include <gb/gb.h>
#include <string.h>
#include "save.h"

#define SAVE_MAGIC 0x43475542UL      /* "BUGC" */
#define SET_MAGIC  0x53544553UL      /* "SETS" */
#define SAVE_VER   5
#define SET_BANK   3
/* MBC5 cart RAM: 0x0A to 0x0000-0x1FFF enables it, the RAM bank number goes to 0x4000-0x5FFF.
   (tests/ overrides these to run the save logic on a PC) */
#ifndef SRAM
#define SRAM ((volatile UINT8 *)0xA000)
#define RAM_ON()    (*(volatile UINT8 *)0x0000 = 0x0A)
#define RAM_OFF()   (*(volatile UINT8 *)0x0000 = 0x00)
#define RAM_BANK(b) (*(volatile UINT8 *)0x4000 = (b))
#endif

Save  sv;
Opts  opt;
UINT8 cur_slot = 0, last_slot = 0;

typedef struct { UINT32 magic; UINT8 last; Opts o; UINT32 sum; } Settings;
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

const UINT8 hold_tab[6] = { 10, 20, 30, 45, 65, 99 };
const UINT8 tank_tab[6] = { 14, 20, 26, 34, 44, 60 };

static volatile UINT32 vbl32;
static UINT32 play_base, play_t0;

static void vbl_isr(void){ vbl32++; }
UINT32 frames32(void){ UINT32 v; CRITICAL { v = vbl32; } return v; }
UINT32 play_secs(void){ return play_base + (frames32() - play_t0) / 60; }
void   play_reset(UINT32 base){ play_base = base; play_t0 = frames32(); }

static UINT32 fnv(const void *d, UINT16 n){
    const UINT8 *p = (const UINT8 *)d; UINT32 h = 2166136261UL;
    while(n--){ h ^= *p++; h *= 16777619UL; }
    return h;
}
static void sram_read(UINT8 bank, void *d, UINT16 n){
    UINT8 *p = (UINT8 *)d; UINT16 i;
    RAM_ON(); RAM_BANK(bank);
    for(i = 0; i < n; i++) p[i] = SRAM[i];
    RAM_OFF();
}
static void sram_write(UINT8 bank, const void *s, UINT16 n){
    const UINT8 *p = (const UINT8 *)s; UINT16 i;
    RAM_ON(); RAM_BANK(bank);
    for(i = 0; i < n; i++) SRAM[i] = p[i];
    RAM_OFF();
}

static void opts_default(void){
    opt.music = 2; opt.sfx = 1; opt.autosave = 1; opt.shake = 1; opt.diff = 1;
    opt.baby = 0; opt.hunger = 1; opt.faint = 1; opt.xpm = 0; opt.goldm = 0;
    opt.cam = 0; opt.minimap = 1; opt.nums = 1; opt.hints = 1; opt.arrow = 1;
}
void settings_save(void){
    Settings s;
    memset(&s, 0, sizeof s);
    s.magic = SET_MAGIC; s.last = last_slot; s.o = opt;
    s.sum = fnv(&s, sizeof s - 4);
    sram_write(SET_BANK, &s, sizeof s);
}
static void settings_load(void){
    Settings s;
    opts_default(); last_slot = 0;
    sram_read(SET_BANK, &s, sizeof s);
    if(s.magic != SET_MAGIC || s.sum != fnv(&s, sizeof s - 4) || s.last >= NSLOT) return;   /* first run: defaults */
    last_slot = s.last; opt = s.o;
    if(opt.music > 2) opt.music = 2;                 /* clamp anything odd */
    if(opt.diff > 4) opt.diff = 4;
    if(opt.xpm > 2) opt.xpm = 2;
    if(opt.goldm > 2) opt.goldm = 2;
    opt.sfx &= 1; opt.autosave &= 1; opt.shake &= 1; opt.baby &= 1; opt.hunger &= 1; opt.faint &= 1;
    opt.cam &= 1; opt.minimap &= 1; opt.nums &= 1; opt.hints &= 1; opt.arrow &= 1;
}

void save_init(void){
    add_VBL(vbl_isr);
    set_interrupts(VBL_IFLAG);
    play_reset(0);
    settings_load();
}

UINT8 slot_peek(UINT8 slot, Save *out){
    Save1 o; Save2 t; Save3 t3; Save4 t4;
    if(slot >= NSLOT) return 0;
    sram_read(slot, out, sizeof(Save));
    if(out->magic == SAVE_MAGIC && out->ver == SAVE_VER && out->sum == fnv(out, sizeof(Save) - 4)) return 1;
    sram_read(slot, &t4, sizeof t4);                                 /* a version 4 save: no Elite layer yet (a full tank, an empty hold) */
    if(t4.magic == SAVE_MAGIC && t4.ver == 4 && t4.sum == fnv(&t4, sizeof t4 - 4)){
        memset(out, 0, sizeof *out);
        memcpy(out, &t4, sizeof t4 - 4); out->ver = SAVE_VER; memset(&out->el, 0, sizeof out->el); out->el.fuel = tank_tab[0];
        return 1;
    }
    sram_read(slot, &t3, sizeof t3);                                 /* a version 3 save: everything is the same, the upgrades start at zero */
    if(t3.magic == SAVE_MAGIC && t3.ver == 3 && t3.sum == fnv(&t3, sizeof t3 - 4)){
        memset(out, 0, sizeof *out);
        memcpy(out, &t3, sizeof t3 - 4); out->ver = SAVE_VER; memset(&out->el, 0, sizeof out->el); memset(out->up, 0, sizeof out->up); out->el.fuel = tank_tab[0];
        return 1;
    }
    sram_read(slot, &t, sizeof t);                                   /* a version 2 save: the same slot, a bug with 13 parts */
    if(t.magic == SAVE_MAGIC && t.ver == 2 && t.sum == fnv(&t, sizeof t - 4)){
        memset(out, 0, sizeof *out);
        out->magic = SAVE_MAGIC; out->ver = SAVE_VER; out->seed = t.seed; out->zone = t.zone; out->level = t.level; out->hp = t.hp; out->mp = t.mp;
        out->gold = t.gold; out->xp = t.xp; out->secs = t.secs; out->saves = t.saves; memcpy(out->name, t.name, 9);
        memcpy(out->look.col, t.col, sizeof t.col); memcpy(out->look.pt, t.pt, 13);
        out->friends = t.friends; out->deepest = t.deepest; out->potions = t.potions; out->star = t.star; out->cores = t.cores; out->story = t.story;
        memcpy(out->freed, t.freed, 8); out->warp_best = t.warp_best; out->el.fuel = tank_tab[0];
        return 1;
    }
    sram_read(slot, &o, sizeof o);                                   /* an old step 3-4 save: keep it, give the bug the CLASSIC look */
    if(o.magic != SAVE_MAGIC || o.ver != 1 || o.sum != fnv(&o, sizeof o - 4)) return 0;
    memset(out, 0, sizeof *out);
    out->magic = SAVE_MAGIC; out->ver = SAVE_VER; out->seed = o.seed; out->zone = o.zone; out->level = o.level; out->hp = o.hp; out->mp = o.mp;
    out->gold = o.gold; out->xp = o.xp; out->secs = o.secs; out->saves = o.saves; memcpy(out->name, o.name, 9);
    bug_default(&out->look); out->el.fuel = tank_tab[0];
    return 1;
}
UINT8 slot_read(UINT8 slot){
    Save t;
    if(!slot_peek(slot, &t)) return 0;
    sv = t;
    return 1;
}
void slot_write(UINT8 slot){
    Save old;
    UINT16 n = slot_peek(slot, &old) ? old.saves : 0;
    sv.magic = SAVE_MAGIC; sv.ver = SAVE_VER;
    sv.secs = play_secs(); sv.saves = n + 1;
    sv.sum = fnv(&sv, sizeof(Save) - 4);
    sram_write(slot, &sv, sizeof(Save));
    if(last_slot != slot){ last_slot = slot; settings_save(); }
}
void slot_erase(UINT8 slot){
    Save z;
    memset(&z, 0, sizeof z);
    sram_write(slot, &z, sizeof z);
    if(last_slot == slot){ last_slot = first_good_slot() == 255 ? 0 : first_good_slot(); settings_save(); }
}
void new_game(UINT8 slot, UINT32 seed, const BugLook *look, const char *name){
    memset(&sv, 0, sizeof sv);
    sv.seed = seed; sv.level = 1; sv.hp = 10; sv.mp = 5; sv.el.fuel = tank_tab[0];
    strcpy(sv.name, name ? name : "BUG");
    if(look) sv.look = *look; else bug_default(&sv.look);
    bug_clamp(&sv.look);
    play_reset(0);
    slot_write(slot);
}
UINT8 first_good_slot(void){
    Save t; UINT8 i;
    if(slot_peek(last_slot, &t)) return last_slot;
    for(i = 0; i < NSLOT; i++) if(slot_peek(i, &t)) return i;
    return 255;
}

UINT32 world_seed(UINT8 star){ return star ? sv.seed ^ ((UINT32)star * 0x9E3779B1UL) : sv.seed; }
UINT8 world_freed(UINT8 star){ UINT8 i; for(i = 0; i < sv.cores && i < 8; i++) if(sv.freed[i] == star) return 1; return 0; }
